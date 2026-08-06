package ui

import (
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"sync/atomic"
	"time"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/widget"

	"luckystar/internal/bridge"
	"luckystar/internal/mcp"
)

const applicationName = "LuckyStar"

type App struct {
	Win fyne.Window
	Br  *bridge.Bridge

	tabs  *container.AppTabs
	header *HeaderPanel

	memoryPage  *MemoryPage
	scanPage    *ScanPage
	savePage    *SavePage
	browserPage *BrowserPage
	pointerPage *PointerPage
	bpPage      *BpPage
	monitorPage *MonitorPage
	envPage     *EnvPage
	sigPage     *SigPage

	statusLabel *widget.Label
	logView     *TextArea

	mcpServer *mcp.Server

	generation atomic.Int64
	closing    atomic.Bool
	live       *LiveRefresher
}

// Shutdown stops background workers; called when the window closes.
func (a *App) Shutdown() {
	a.closing.Store(true)
	a.live.Stop()
	if a.mcpServer != nil {
		a.mcpServer.Stop()
		a.mcpServer = nil
	}
	a.Br.Disconnect()
}

// snapshotLiveTask picks the refresh task for the currently active tab.
// Must run on the UI thread.
func (a *App) snapshotLiveTask() *liveTask {
	if a.closing.Load() {
		return nil
	}
	index := a.tabs.SelectedIndex()
	switch index {
	case 1: // 扫描页
		return a.scanPage.liveRefreshSnapshot()
	case 2: // 保存页
		return &liveTask{kind: "saved"}
	case 4: // 指针页
		return &liveTask{kind: "pointer"}
	case 5: // 断点页
		return &liveTask{kind: "breakpoint"}
	case 6: // 监控页
		return &liveTask{kind: "monitor"}
	}
	return nil
}

func NewApp(win fyne.Window) *App {
	a := &App{Win: win, Br: bridge.New(bridge.DefaultTimeoutSeconds)}
	a.statusLabel = widget.NewLabel("客户端已启动")
	a.logView = NewTextArea()
	a.logView.SetReadOnly(true)

	a.header = newHeaderPanel(a)
	a.memoryPage = newMemoryPage(a)
	a.scanPage = newScanPage(a)
	a.savePage = newSavePage(a)
	a.browserPage = newBrowserPage(a)
	a.pointerPage = newPointerPage(a)
	a.bpPage = newBpPage(a)
	a.monitorPage = newMonitorPage(a)
	a.envPage = newEnvPage(a)
	a.sigPage = newSigPage(a)

	a.tabs = container.NewAppTabs(
		container.NewTabItem("内存信息页", a.memoryPage.Content()),
		container.NewTabItem("扫描页", a.scanPage.Content()),
		container.NewTabItem("保存页", a.savePage.Content()),
		container.NewTabItem("内存浏览页", a.browserPage.Content()),
		container.NewTabItem("指针页", a.pointerPage.Content()),
		container.NewTabItem("断点页", a.bpPage.Content()),
		container.NewTabItem("监控页", a.monitorPage.Content()),
		container.NewTabItem("环境参数页", a.envPage.Content()),
		container.NewTabItem("特征码页", a.sigPage.Content()),
		container.NewTabItem("日志页", a.logPageContent()),
	)
	a.tabs.OnSelected = func(*container.TabItem) {
		a.live.Resume()
		a.live.TickNow()
	}

	a.live = NewLiveRefresher(a)

	root := container.NewBorder(
		a.header.Content(),
		nil, nil, nil,
		a.tabs,
	)
	win.SetContent(root)
	a.log("客户端已启动。")
	return a
}

func (a *App) logPageContent() fyne.CanvasObject {
	clearBtn := widget.NewButton("清空日志", func() { a.logView.Display("") })
	top := container.NewHBox(clearBtn)
	card := widget.NewCard("运行日志", "", a.logView)
	return container.NewBorder(top, nil, nil, nil, card)
}

func (a *App) setStatus(text string) {
	a.statusLabel.SetText(text)
	a.log("状态: " + text)
}

func (a *App) log(text string) {
	timestamp := time.Now().Format("15:04:05")
	a.logView.Display(a.logView.Text + "\n[" + timestamp + "] " + text)
}

// runOp sends a bridge operation on a worker goroutine and applies the
// result on the UI thread. Bridge-level errors become a synthetic failed
// envelope; connection failures only update the status.
func (a *App) runOp(operation string, params map[string]any, apply func(resp *bridge.Response)) {
	if !a.Br.IsConnected() {
		a.setStatus("尚未建立通信，请先点击“测试通信”")
		return
	}
	a.log(fmt.Sprintf("发送操作: %s %s", operation, compactJSON(params)))
	go func() {
		resp, err := a.Br.CallOperation(operation, params)
		if err == nil {
			fyne.Do(func() {
				a.log(fmt.Sprintf("收到响应: ok=%v operation=%s", resp.Ok, resp.Operation))
				apply(resp)
			})
			return
		}
		var opErr *bridge.OpError
		if errors.As(err, &opErr) && (opErr.Kind == bridge.KindGeneric || opErr.Kind == bridge.KindProtocol) {
			synthetic := &bridge.Response{Ok: false, Operation: operation, Error: opErr.Msg}
			fyne.Do(func() { apply(synthetic) })
			return
		}
		fyne.Do(func() { a.setStatus("请求失败：" + err.Error()) })
	}()
}

// runOpQuiet is runOp without request/response logging (used by live refresh).
func (a *App) runOpQuiet(operation string, params map[string]any, apply func(resp *bridge.Response)) {
	if !a.Br.IsConnected() {
		return
	}
	go func() {
		resp, err := a.Br.CallOperation(operation, params)
		if err != nil {
			var opErr *bridge.OpError
			if errors.As(err, &opErr) && (opErr.Kind == bridge.KindGeneric || opErr.Kind == bridge.KindProtocol) {
				synthetic := &bridge.Response{Ok: false, Operation: operation, Error: opErr.Msg}
				fyne.Do(func() { apply(synthetic) })
			}
			return
		}
		fyne.Do(func() { apply(resp) })
	}()
}

func compactJSON(v any) string {
	data, err := json.Marshal(v)
	if err != nil {
		return fmt.Sprint(v)
	}
	return string(data)
}

// response helpers

func respOK(resp *bridge.Response) bool {
	return resp != nil && resp.Ok
}

func respError(resp *bridge.Response) string {
	if resp == nil {
		return "无响应"
	}
	if resp.Error != "" {
		return resp.Error
	}
	return "未知错误"
}

func respData(resp *bridge.Response) map[string]any {
	if resp != nil && resp.Ok {
		if m, ok := resp.Data.(map[string]any); ok {
			return m
		}
	}
	return map[string]any{}
}

// dialogs

func (a *App) warn(title, message string) {
	dialogShowWarning(title, message, a.Win)
}

func (a *App) info(title, message string) {
	dialogShowInformation(title, message, a.Win)
}

func (a *App) confirm(title, message string, onConfirm func()) {
	dialogShowConfirm(title, message, onConfirm, a.Win)
}

func (a *App) askText(title, message, initial string, onConfirm func(text string)) {
	dialogAskText(title, message, initial, onConfirm, a.Win)
}

// disconnectDevice tears down the bridge and resets all page state.
func (a *App) disconnectDevice(reason string) {
	a.Br.Disconnect()
	a.invalidateTargetUI()
	a.header.setConnected(false)
	if reason != "" {
		a.setStatus(reason)
	}
}

func (a *App) invalidateTargetUI() {
	a.generation.Add(1)
	a.header.setPid(0)
	a.memoryPage.invalidate()
	a.scanPage.invalidate()
	a.savePage.invalidate()
	a.browserPage.invalidate()
	a.pointerPage.invalidate()
	a.bpPage.invalidate()
	a.monitorPage.invalidate()
	a.envPage.invalidate()
	a.sigPage.invalidate()
}

// notifyIfOpFailed shows a warning dialog when a response envelope is not ok.
func (a *App) notifyIfOpFailed(resp *bridge.Response, title, prefix string) bool {
	if respOK(resp) {
		return true
	}
	a.warn(title, prefix+respError(resp))
	return false
}

func trim(s string) string { return strings.TrimSpace(s) }
