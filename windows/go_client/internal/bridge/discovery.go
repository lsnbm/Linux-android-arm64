package bridge

import (
	"context"
	"encoding/binary"
	"net"
	"os/exec"
	"regexp"
	"sort"
	"strings"
	"sync"
	"syscall"
	"time"
)

const (
	LANDiscoveryMaxWorkers    = 24
	LANDiscoveryPingTimeoutMS = 150
)

const createNoWindow = 0x08000000

type LanDevice struct {
	Host string
	Mac  string
}

// DiscoverLanDevices pings the local /24 subnets then reads the ARP table.
// Mirrors discover_lan_devices from http_bridge.py.
func DiscoverLanDevices() []LanDevice {
	localIPs := collectLocalIPv4()
	if len(localIPs) == 0 {
		return nil
	}
	targets, localSet := subnetTargets(localIPs)
	if len(targets) > 0 {
		sem := make(chan struct{}, LANDiscoveryMaxWorkers)
		var wg sync.WaitGroup
		for ip := range targets {
			ip := ip
			wg.Add(1)
			sem <- struct{}{}
			go func() {
				defer wg.Done()
				defer func() { <-sem }()
				pingHost(ip)
			}()
		}
		wg.Wait()
	}

	arp := readARPTable()
	var devices []LanDevice
	for ip, mac := range arp {
		if localSet[ip] {
			continue
		}
		if len(targets) > 0 && !targets[ip] {
			continue
		}
		devices = append(devices, LanDevice{Host: ip, Mac: mac})
	}
	sort.Slice(devices, func(i, j int) bool {
		return ipToUint32(devices[i].Host) < ipToUint32(devices[j].Host)
	})
	return devices
}

func collectLocalIPv4() []string {
	seen := map[string]bool{}
	ifs, err := net.Interfaces()
	if err == nil {
		for _, ifc := range ifs {
			addrs, err := ifc.Addrs()
			if err != nil {
				continue
			}
			for _, a := range addrs {
				ipNet, ok := a.(*net.IPNet)
				if !ok {
					continue
				}
				ip := ipNet.IP.To4()
				if ip == nil || ip.IsLoopback() {
					continue
				}
				if !isPrivateIPv4(ip) {
					continue
				}
				seen[ip.String()] = true
			}
		}
	}
	// UDP probe fallback (mirrors the original).
	if len(seen) == 0 {
		if conn, err := net.Dial("udp4", "114.114.114.114:53"); err == nil {
			if ip := conn.LocalAddr().(*net.UDPAddr).IP.To4(); ip != nil && !ip.IsLoopback() && isPrivateIPv4(ip) {
				seen[ip.String()] = true
			}
			conn.Close()
		}
	}
	ips := make([]string, 0, len(seen))
	for ip := range seen {
		ips = append(ips, ip)
	}
	sort.Strings(ips)
	return ips
}

func isPrivateIPv4(ip net.IP) bool {
	ip4 := ip.To4()
	if ip4 == nil {
		return false
	}
	return ip4[0] == 10 ||
		(ip4[0] == 172 && ip4[1] >= 16 && ip4[1] <= 31) ||
		(ip4[0] == 192 && ip4[1] == 168)
}

func subnetTargets(localIPs []string) (map[string]bool, map[string]bool) {
	targets := map[string]bool{}
	localSet := map[string]bool{}
	for _, ip := range localIPs {
		localSet[ip] = true
		parsed := net.ParseIP(ip).To4()
		if parsed == nil {
			continue
		}
		for host := 1; host <= 254; host++ {
			addr := net.IPv4(parsed[0], parsed[1], parsed[2], byte(host))
			if !localSet[addr.String()] {
				targets[addr.String()] = true
			}
		}
	}
	return targets, localSet
}

func pingHost(ip string) bool {
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	cmd := exec.CommandContext(ctx, "ping", "-n", "1", "-w", "150", ip)
	cmd.SysProcAttr = &syscall.SysProcAttr{CreationFlags: createNoWindow}
	cmd.Stdout = nil
	cmd.Stderr = nil
	err := cmd.Run()
	return err == nil
}

var arpPattern = regexp.MustCompile(`(\d+\.\d+\.\d+\.\d+)\s+([0-9A-Fa-f-]{17})\s+\S+`)

func readARPTable() map[string]string {
	result := map[string]string{}
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	cmd := exec.CommandContext(ctx, "arp", "-a")
	cmd.SysProcAttr = &syscall.SysProcAttr{CreationFlags: createNoWindow}
	out, err := cmd.Output()
	if err != nil {
		return result
	}
	for _, match := range arpPattern.FindAllStringSubmatch(string(out), -1) {
		if len(match) >= 3 {
			result[match[1]] = strings.ToLower(match[2])
		}
	}
	return result
}

func ipToUint32(ip string) uint32 {
	parsed := net.ParseIP(ip).To4()
	if parsed == nil {
		return 0
	}
	return binary.BigEndian.Uint32(parsed)
}
