package ui

import (
	"strings"
	"sync/atomic"
	"time"

	"fyne.io/fyne/v2"

	"luckystar/internal/bridge"
)

type liveTask struct {
	kind          string // "scan", "saved", "pointer", "breakpoint", "monitor"
	scanStart     int64
	scanCount     int64
	scanValueType string
}

type liveResult struct {
	task       *liveTask
	generation int64
	connError  string

	scanGet  map[string]any
	scanPage map[string]any

	savedData map[string]any

	pointerData map[string]any

	breakpointData map[string]any

	syscallLog   string
	syscallLines int64
	cntvctLog    string
	cntvctLines  int64
}

type LiveRefresher struct {
	app      *App
	stopCh   chan struct{}
	inflight atomic.Bool
	paused   atomic.Bool
}

func NewLiveRefresher(a *App) *LiveRefresher {
	l := &LiveRefresher{app: a, stopCh: make(chan struct{})}
	go l.loop()
	return l
}

func (l *LiveRefresher) Stop() {
	select {
	case <-l.stopCh:
	default:
		close(l.stopCh)
	}
}

func (l *LiveRefresher) loop() {
	ticker := time.NewTicker(time.Second)
	defer ticker.Stop()
	for {
		select {
		case <-l.stopCh:
			return
		case <-ticker.C:
			l.TickNow()
		}
	}
}

// Resume clears the paused state so background refresh resumes on the
// next tick. Called after tab changes and successful connections.
func (l *LiveRefresher) Resume() {
	l.paused.Store(false)
}

// TickNow runs one background refresh cycle. Safe to call from any goroutine.
func (l *LiveRefresher) TickNow() {
	a := l.app
	if a.closing.Load() || !a.Br.IsConnected() {
		return
	}
	if !l.inflight.CompareAndSwap(false, true) {
		return
	}
	var task *liveTask
	fyne.Do(func() { task = a.snapshotLiveTask() })
	if task == nil || l.paused.Load() {
		l.inflight.Store(false)
		return
	}
	gen := a.generation.Load()
	result := l.runTask(task, gen)
	fyne.Do(func() { l.applyResult(result) })
	l.inflight.Store(false)
}

// runTask performs the bridge calls on the current goroutine.
func (l *LiveRefresher) runTask(task *liveTask, gen int64) *liveResult {
	a := l.app
	result := &liveResult{task: task, generation: gen}
	switch task.kind {
	case "scan":
		getResp, err := a.Br.CallOperation("scan.get", nil)
		if err != nil {
			result.connError = err.Error()
			return result
		}
		if getResp.Ok {
			getData := dataMapOf(getResp)
			result.scanGet = getData
			if !toBool(getData["scanning"]) {
				pageResp, err := a.Br.CallOperation("scan.results", map[string]any{
					"start":      task.scanStart,
					"count":      task.scanCount,
					"value_type": task.scanValueType,
				})
				if err == nil && pageResp.Ok {
					result.scanPage = dataMapOf(pageResp)
				}
			}
		}
	case "saved":
		resp, err := a.Br.CallOperation("saved.list", nil)
		if err != nil {
			result.connError = err.Error()
			return result
		}
		if resp.Ok {
			result.savedData = dataMapOf(resp)
		}
	case "pointer":
		resp, err := a.Br.CallOperation("pointer.get", nil)
		if err != nil {
			result.connError = err.Error()
			return result
		}
		if resp.Ok {
			result.pointerData = dataMapOf(resp)
		}
	case "breakpoint":
		resp, err := a.Br.CallOperation("breakpoint.get", nil)
		if err != nil {
			result.connError = err.Error()
			return result
		}
		if resp.Ok {
			result.breakpointData = dataMapOf(resp)
		}
	case "monitor":
		if resp, err := a.Br.CallOperation("syscall.read", nil); err == nil && resp.Ok {
			data := dataMapOf(resp)
			result.syscallLog = strings.TrimRight(toString(data["log"]), "\n")
			result.syscallLines = toInt64(data["line_count"])
		}
		if resp, err := a.Br.CallOperation("cntvct.read", nil); err == nil && resp.Ok {
			data := dataMapOf(resp)
			result.cntvctLog = strings.TrimRight(toString(data["log"]), "\n")
			result.cntvctLines = toInt64(data["line_count"])
		}
	}
	return result
}

func dataMapOf(resp *bridge.Response) map[string]any {
	if m, ok := resp.Data.(map[string]any); ok {
		return m
	}
	return map[string]any{}
}

func (l *LiveRefresher) applyResult(result *liveResult) {
	a := l.app
	if result.generation != a.generation.Load() {
		return
	}
	if result.connError != "" {
		l.paused.Store(true)
		a.setStatus("后台刷新失败：" + result.connError)
		return
	}
	switch result.task.kind {
	case "scan":
		a.scanPage.applyLiveRefreshScan(result.scanGet, result.scanPage)
	case "saved":
		if result.savedData != nil {
			a.savePage.adoptState(result.savedData)
		}
	case "pointer":
		if result.pointerData != nil {
			a.pointerPage.applyStatusData(result.pointerData, true)
		}
	case "breakpoint":
		if result.breakpointData != nil {
			a.bpPage.renderBPInfo(result.breakpointData)
		}
	case "monitor":
		a.monitorPage.syscall.logView.Display(result.syscallLog)
		a.monitorPage.cntvct.logView.Display(result.cntvctLog)
	}
}
