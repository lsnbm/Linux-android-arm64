package ui

import (
	"errors"
	"fmt"
	"image/color"
	"strconv"
	"strings"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/widget"

	"luckystar/internal/bridge"
	"luckystar/internal/mcp"
)

type HeaderPanel struct {
	app *App

	deviceEntry *widget.SelectEntry
	portEntry   *widget.Entry
	pidEntry    *widget.Entry

	scanDevBtn *widget.Button
	connectBtn *widget.Button
	syncBtn    *widget.Button

	mcpBtn     *widget.Button
	mcpPort    *widget.Entry
	mcpLabel   *widget.Label
	mcpRunning bool

	badge     *canvas.Text
	pidLabel  *widget.Label
	statusRow *widget.Label

	deviceOptions []string
	deviceData    []string
	content       fyne.CanvasObject
}

func newHeaderPanel(a *App) *HeaderPanel {
	h := &HeaderPanel{app: a}

	h.deviceEntry = widget.NewSelectEntry(nil)
	h.deviceEntry.SetPlaceHolder("设备 IP 或 https://xxxx.trycloudflare.com")
	h.portEntry = widget.NewEntry()
	h.portEntry.SetText(strconv.Itoa(bridge.DefaultPort))
	h.portEntry.SetPlaceHolder("端口")
	h.pidEntry = widget.NewEntry()
	h.pidEntry.SetPlaceHolder("例如 12345 或 me.hd.ggtutorial")
	h.pidEntry.OnSubmitted = func(string) { h.onSyncPid() }

	h.scanDevBtn = widget.NewButton("扫描设备", h.onScanDevices)
	h.connectBtn = widget.NewButton("测试通信", h.onTestCommunication)
	h.syncBtn = widget.NewButton("同步 PID", h.onSyncPid)

	h.badge = canvas.NewText("● 未通信", color.RGBA{R: 0x9E, G: 0xA3, B: 0xAF, A: 0xFF})
	h.pidLabel = widget.NewLabel("--")
	h.statusRow = widget.NewLabel("客户端已启动")

	// row 1: device scan / endpoint
	row1 := container.NewHBox(
		h.scanDevBtn,
		fixWidth(h.deviceEntry, 480),
		widget.NewLabel("端口"),
		fixWidth(h.portEntry, 110),
		h.connectBtn,
	)

	// row 2: target
	row2 := container.NewHBox(
		widget.NewLabel("PID / 包名"),
		fixWidth(h.pidEntry, 400),
		h.syncBtn,
	)

	// row 3: status
	row3 := container.NewHBox(
		h.badge,
		widget.NewLabel("PID"),
		h.pidLabel,
		widget.NewLabel("状态"),
		fixWidth(h.statusRow, 620),
	)

	// row 4: MCP service control
	h.mcpPort = widget.NewEntry()
	h.mcpPort.SetText(strconv.Itoa(mcp.DefaultBindPort))
	h.mcpPort.SetPlaceHolder("端口")
	h.mcpLabel = widget.NewLabel("MCP 服务: 未启动")
	h.mcpBtn = widget.NewButton("启动 MCP 服务", h.onToggleMCP)
	row4 := container.NewHBox(
		h.mcpBtn,
		widget.NewLabel("MCP 端口"),
		fixWidth(h.mcpPort, 110),
		fixWidth(h.mcpLabel, 420),
	)

	header := container.NewVBox(row1, row2, row3, row4)
	h.content = widget.NewCard("连接", "", header)
	return h
}

// onToggleMCP starts or stops the embedded MCP server.
func (h *HeaderPanel) onToggleMCP() {
	a := h.app
	if h.mcpRunning {
		if a.mcpServer != nil {
			a.mcpServer.Stop()
			a.mcpServer = nil
		}
		h.mcpRunning = false
		h.mcpBtn.SetText("启动 MCP 服务")
		h.mcpLabel.SetText("MCP 服务: 未启动")
		a.setStatus("MCP 服务已停止")
		return
	}
	port, err := strconv.Atoi(strings.TrimSpace(h.mcpPort.Text))
	if err != nil || port < 1 || port > 65535 {
		a.warn("输入提示", "MCP 端口必须是 1..65535 之间的整数。")
		return
	}
	h.mcpBtn.Disable()
	h.mcpBtn.SetText("正在启动...")
	go func() {
		srv, err := mcp.Start(mcp.Config{
			BindHost:       "127.0.0.1",
			BindPort:       port,
			Path:           mcp.DefaultPath,
			AndroidHost:    "auto",
			AndroidPort:    bridge.DefaultPort,
			AndroidTimeout: bridge.DefaultTimeoutSeconds,
		})
		fyne.Do(func() {
			h.mcpBtn.Enable()
			if err != nil {
				h.mcpLabel.SetText("MCP 服务: 启动失败")
				a.warn("MCP 启动失败", err.Error())
				a.setStatus("MCP 服务启动失败：" + err.Error())
				return
			}
			a.mcpServer = srv
			h.mcpRunning = true
			h.mcpBtn.SetText("停止 MCP 服务")
			h.mcpLabel.SetText("MCP 服务: " + srv.URL())
			a.setStatus("MCP 服务已启动: " + srv.URL())
			a.log("MCP 服务已启动: " + srv.URL())
		})
	}()
}

func (h *HeaderPanel) Content() fyne.CanvasObject { return h.content }

func (h *HeaderPanel) setPid(pid int64) {
	if pid > 0 {
		h.pidLabel.SetText(fmt.Sprintf("%d", pid))
	} else {
		h.pidLabel.SetText("--")
	}
}

func (h *HeaderPanel) setConnected(connected bool) {
	if connected {
		h.badge.Text = "● 已通信"
		h.badge.Color = color.RGBA{R: 0x1F, G: 0xA8, B: 0x5A, A: 0xFF}
	} else {
		h.badge.Text = "● 未通信"
		h.badge.Color = color.RGBA{R: 0x9E, G: 0xA3, B: 0xAF, A: 0xFF}
	}
	h.badge.Refresh()
}

// currentDeviceHost returns the device text or, when an option is selected,
// its underlying IP data.
func (h *HeaderPanel) currentDeviceHost() string {
	text := strings.TrimSpace(h.deviceEntry.Text)
	for i, option := range h.deviceOptions {
		if text == option && i < len(h.deviceData) {
			return h.deviceData[i]
		}
	}
	return text
}

func (h *HeaderPanel) parseEndpoint() (string, int, bool) {
	host := h.currentDeviceHost()
	if host == "" {
		h.app.warn("输入提示", "请输入设备 IP、HTTP(S) URL，或先扫描局域网设备。")
		return "", 0, false
	}
	if strings.Contains(host, "://") {
		return host, bridge.DefaultPort, true
	}
	portText := strings.TrimSpace(h.portEntry.Text)
	port, err := strconv.Atoi(portText)
	if err != nil || port <= 0 || port > 65535 {
		h.app.warn("输入提示", "端口必须是 1..65535 之间的整数。")
		return "", 0, false
	}
	return host, port, true
}

func (h *HeaderPanel) onScanDevices() {
	if h.scanDevBtn.Disabled() {
		return
	}
	h.scanDevBtn.Disable()
	h.scanDevBtn.SetText("扫描中...")
	previous := h.currentDeviceHost()
	h.deviceEntry.SetOptions(nil)
	h.deviceEntry.SetText("正在扫描局域网设备，请稍候...")
	go func() {
		devices := bridge.DiscoverLanDevices()
		fyne.Do(func() {
			h.finishScanDevices(devices, previous)
		})
	}()
}

func (h *HeaderPanel) finishScanDevices(devices []bridge.LanDevice, previous string) {
	h.scanDevBtn.Enable()
	h.scanDevBtn.SetText("扫描设备")
	var options []string
	var data []string
	switch {
	case len(devices) == 0:
		if previous == "" {
			h.app.setStatus("未发现设备，请确认手机与电脑在同一网段后重试")
		} else {
			h.app.setStatus("未发现新设备，已保留之前输入的地址")
		}
	default:
		for _, device := range devices {
			label := device.Host
			if device.Mac != "" {
				label = fmt.Sprintf("%s    [%s]", device.Host, device.Mac)
			}
			options = append(options, label)
			data = append(data, device.Host)
		}
		h.app.setStatus(fmt.Sprintf("发现 %d 个设备", len(devices)))
	}
	h.deviceOptions = options
	h.deviceData = data
	h.deviceEntry.SetOptions(options)
	if previous != "" {
		h.deviceEntry.SetText(previous)
	} else if len(data) > 0 {
		h.deviceEntry.SetText(options[0])
	}
}

func (h *HeaderPanel) onTestCommunication() {
	a := h.app
	host, port, ok := h.parseEndpoint()
	if !ok {
		return
	}
	if err := a.Br.Connect(host, port); err != nil {
		a.warn("通信失败", err.Error())
		return
	}
	a.setStatus(fmt.Sprintf("正在连接 %s:%d ...", host, port))
	go func() {
		_, err := a.Br.CallOperation("bridge.ping", nil)
		if err != nil {
			var opErr *bridge.OpError
			if errors.As(err, &opErr) && (opErr.Kind == bridge.KindGeneric || opErr.Kind == bridge.KindProtocol) {
				fyne.Do(func() {
					a.disconnectDevice("")
					message := "协议不兼容"
					lower := strings.ToLower(err.Error())
					if strings.Contains(lower, "未知 operation") || strings.Contains(lower, "unknown operation") {
						message = "Android 端 HTTP 程序版本过旧，请重新编译并部署当前源码中的可执行程序。"
					}
					a.warn("协议不兼容", message)
				})
				return
			}
			fyne.Do(func() {
				a.disconnectDevice("连接失败：" + err.Error())
			})
			return
		}
		targetResp, targetErr := a.Br.CallOperation("target.get", nil)
		fyne.Do(func() {
			if targetErr != nil || !targetResp.Ok {
				a.disconnectDevice("连接失败：Android 端未返回有效目标信息")
				return
			}
			h.setConnected(true)
			data := respData(targetResp)
			pid := toInt64(data["pid"])
			h.setPid(pid)
			a.savePage.refreshState(true)
			a.setStatus("通信成功：" + a.Br.URL())
			a.live.Resume()
			a.live.TickNow()
		})
	}()
}

func (h *HeaderPanel) onSyncPid() {
	a := h.app
	input := strings.TrimSpace(h.pidEntry.Text)
	if input == "" {
		a.warn("输入提示", "请输入 PID 或包名。")
		return
	}
	var operation string
	var params map[string]any
	if isAllDigits(input) {
		pid, err := strconv.ParseInt(input, 10, 64)
		if err != nil || pid <= 0 {
			a.warn("输入提示", "PID 必须是大于 0 的整数。")
			return
		}
		operation = "target.select"
		params = map[string]any{"pid": pid}
	} else {
		operation = "target.attach"
		params = map[string]any{"package_name": input}
	}
	a.runOp(operation, params, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "同步失败", "同步 PID 失败：") {
			a.setStatus("同步失败：请查看服务端返回的具体原因")
			return
		}
		data := respData(resp)
		pid := toInt64(data["pid"])
		if pid <= 0 {
			a.warn("同步失败", "服务端返回的 PID 无效。")
			return
		}
		a.invalidateTargetUI()
		h.setPid(pid)
		a.savePage.refreshState(true)
		a.setStatus(fmt.Sprintf("同步成功：全局PID=%d", pid))
		a.live.TickNow()
	})
}

func isAllDigits(s string) bool {
	for _, r := range s {
		if r < '0' || r > '9' {
			return false
		}
	}
	return s != ""
}
