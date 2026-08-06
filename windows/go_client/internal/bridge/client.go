package bridge

import (
	"fmt"
	"strings"
	"sync"
)

var autoHostTokens = map[string]bool{"": true, "auto": true, "*": true}

// Client mirrors AndroidHttpClient from http_bridge.py: a configurable
// HTTP client with LAN auto-discovery and target-PID tracking. It is used
// by the MCP server and scripted flows.
type Client struct {
	lock     sync.RWMutex
	host     string
	port     int
	timeout  float64
	lastHost string
	devices  []LanDevice
	targetPID int64
	hasTarget bool
	targetHost string
}

func NewClient(host string, port int, timeoutSeconds float64) *Client {
	if host == "" {
		host = "auto"
	}
	if port <= 0 {
		port = DefaultPort
	}
	if timeoutSeconds <= 0 {
		timeoutSeconds = DefaultTimeoutSeconds
	}
	return &Client{host: host, port: port, timeout: timeoutSeconds}
}

func IsAutoHost(host string) bool {
	return autoHostTokens[strings.ToLower(strings.TrimSpace(host))]
}

func (c *Client) ConnectionState() map[string]any {
	c.lock.RLock()
	defer c.lock.RUnlock()
	return c.snapshotLocked()
}

func (c *Client) snapshotLocked() map[string]any {
	devices := make([]map[string]string, 0, len(c.devices))
	for _, device := range c.devices {
		devices = append(devices, map[string]string{"host": device.Host, "mac": device.Mac})
	}
	resolvedHost := any(nil)
	if c.lastHost != "" {
		resolvedHost = c.lastHost
	}
	targetPID := any(nil)
	if c.hasTarget {
		targetPID = c.targetPID
	}
	targetHost := any(nil)
	if c.hasTarget {
		targetHost = c.targetHost
	}
	return map[string]any{
		"host":                   c.host,
		"port":                   c.port,
		"timeout_seconds":        c.timeout,
		"last_connected_host":    c.lastHost,
		"last_discovered_devices": devices,
		"auto_discover":          IsAutoHost(c.host),
		"resolved_host":          resolvedHost,
		"target_pid":             targetPID,
		"target_host":            targetHost,
	}
}

func (c *Client) Configure(host string, port int, timeoutSeconds float64) error {
	c.lock.Lock()
	defer c.lock.Unlock()
	normalizedHost := strings.TrimSpace(host)
	if normalizedHost == "" {
		normalizedHost = "auto"
	}
	if normalizedHost != c.host {
		c.host = normalizedHost
		c.lastHost = ""
		c.devices = nil
		c.targetPID = 0
		c.hasTarget = false
		c.targetHost = ""
	}
	if port > 0 {
		if port > 65535 {
			return fmt.Errorf("port must be in 1..65535")
		}
		if port != c.port {
			c.port = port
			c.targetPID = 0
			c.hasTarget = false
			c.targetHost = ""
		}
	}
	if timeoutSeconds > 0 {
		c.timeout = timeoutSeconds
	}
	return nil
}

// Discover returns the connection snapshot plus bridge candidates.
func (c *Client) Discover() map[string]any {
	c.lock.RLock()
	host := c.host
	lastHost := c.lastHost
	c.lock.RUnlock()

	autoDiscover := IsAutoHost(host)
	var devices []LanDevice
	if autoDiscover {
		devices = DiscoverLanDevices()
	}

	c.lock.Lock()
	c.devices = devices
	snapshot := c.snapshotLocked()
	c.lock.Unlock()

	if autoDiscover {
		snapshot["candidates"] = uniqueHosts([]string{lastHost}, deviceHosts(devices))
	} else {
		snapshot["candidates"] = []string{host}
	}
	return snapshot
}

// CallOperation sends an operation, trying the last-known host first, then
// the configured host, then LAN-discovered devices when auto mode is on.
func (c *Client) CallOperation(operation string, params map[string]any) (*Response, error) {
	if params == nil {
		params = map[string]any{}
	}
	request := map[string]any{"operation": strings.TrimSpace(operation), "params": params}

	c.lock.RLock()
	configuredHost := c.host
	port := c.port
	timeout := c.timeout
	lastHost := c.lastHost
	c.lock.RUnlock()

	autoDiscover := IsAutoHost(configuredHost)
	hosts := []string{}
	if autoDiscover {
		if lastHost != "" {
			hosts = append(hosts, lastHost)
		}
	} else {
		hosts = append(hosts, configuredHost)
	}

	var errorsList []string
	tryHosts := func(candidates []string) (*Response, error) {
		for _, candidate := range candidates {
			response, err := c.requestHost(candidate, port, timeout, configuredHost, request)
			if err != nil {
				var opErr *OpError
				if errorsAs(err, &opErr) && opErr.Kind == KindOutcomeUnknown {
					return nil, err
				}
				errorsList = append(errorsList, fmt.Sprintf("%s:%d -> %s", candidate, port, err))
				continue
			}
			return response, nil
		}
		return nil, nil
	}

	response, err := tryHosts(hosts)
	if err != nil {
		return nil, err
	}
	if response != nil {
		return response, nil
	}

	if autoDiscover {
		devices := DiscoverLanDevices()
		c.lock.Lock()
		c.devices = devices
		c.lock.Unlock()
		skipped := map[string]bool{}
		for _, host := range hosts {
			skipped[host] = true
		}
		var candidates []string
		for _, device := range devices {
			if !skipped[device.Host] {
				candidates = append(candidates, device.Host)
			}
		}
		response, err = tryHosts(candidates)
		if err != nil {
			return nil, err
		}
		if response != nil {
			return response, nil
		}
	}

	if len(errorsList) > 0 {
		return nil, NewError(KindConnection, "failed to reach Android HTTP bridge candidates: %s", strings.Join(errorsList, "; "))
	}
	return nil, NewError(KindConnection, "failed to discover any Android HTTP bridge candidates")
}

func (c *Client) requestHost(host string, port int, timeoutSeconds float64, configuredHost string, request map[string]any) (*Response, error) {
	c.lock.Lock()
	operation := toStringOp(request["operation"])
	if c.hasTarget && c.targetHost != host {
		previousHost := c.targetHost
		c.targetPID = 0
		c.hasTarget = false
		c.targetHost = ""
		if operation != "bridge.ping" && operation != "target.find" &&
			operation != "target.select" && operation != "target.attach" && operation != "target.get" {
			c.lock.Unlock()
			return nil, NewError(KindGeneric,
				"resolved Android device changed from %s to %s; please set or attach the target again",
				previousHost, host)
		}
	}

	bridge := New(timeoutSeconds)
	if err := bridge.Connect(host, port); err != nil {
		c.lock.Unlock()
		return nil, err
	}
	response, err := bridge.request(request)
	if err != nil {
		c.lock.Unlock()
		return nil, err
	}
	c.lastHost = host
	if response.Connection != nil {
		response.Connection["host"] = configuredHost
		response.Connection["url"] = bridge.URL()
		response.Connection["auto_discover"] = IsAutoHost(configuredHost)
	}
	if response.Ok && (operation == "target.select" || operation == "target.attach" || operation == "target.get") {
		if data, ok := response.Data.(map[string]any); ok {
			if pid, err := toInt(data["pid"]); err == nil {
				c.targetPID = pid
				c.hasTarget = true
				c.targetHost = host
			}
		}
	}
	if c.hasTarget {
		response.Connection["target_pid"] = c.targetPID
	} else {
		response.Connection["target_pid"] = nil
	}
	c.lock.Unlock()
	return response, nil
}

// ---- helpers ----

func toStringOp(v any) string {
	if v == nil {
		return ""
	}
	return fmt.Sprint(v)
}

func errorsAs(err error, target any) bool {
	switch t := target.(type) {
	case **OpError:
		opErr, ok := err.(*OpError)
		if !ok {
			return false
		}
		*t = opErr
		return true
	}
	return false
}

func uniqueHosts(groups ...[]string) []string {
	seen := map[string]bool{}
	var ordered []string
	for _, group := range groups {
		for _, host := range group {
			text := strings.TrimSpace(host)
			if text == "" || seen[text] {
				continue
			}
			seen[text] = true
			ordered = append(ordered, text)
		}
	}
	return ordered
}

func deviceHosts(devices []LanDevice) []string {
	hosts := make([]string, 0, len(devices))
	for _, device := range devices {
		hosts = append(hosts, device.Host)
	}
	return hosts
}
