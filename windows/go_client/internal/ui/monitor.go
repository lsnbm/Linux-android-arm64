package ui

import (
	"strings"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/widget"

	"luckystar/internal/bridge"
)

type monitorSection struct {
	app *App
	op  string

	statusLabel *widget.Label
	startBtn    *widget.Button
	stopBtn     *widget.Button
	refreshBtn  *widget.Button
	clearBtn    *widget.Button
	logView     *TextArea

	active bool
}

func (m *monitorSection) build() fyne.CanvasObject {
	m.statusLabel = widget.NewLabel("未监听")
	m.startBtn = widget.NewButton("开始监听", m.onStart)
	m.stopBtn = widget.NewButton("停止监听", m.onStop)
	refreshBtn := widget.NewButton("刷新日志", m.onRefresh)
	clearBtn := widget.NewButton("清空显示", func() { m.logView.Display("") })
	m.refreshBtn = refreshBtn
	m.clearBtn = clearBtn
	m.logView = NewTextArea()
	m.logView.SetReadOnly(true)

	row := container.NewHBox(m.statusLabel, m.startBtn, m.stopBtn, refreshBtn, clearBtn)
	m.applyState()
	return container.NewBorder(container.NewVBox(row), nil, nil, nil, m.logView)
}

func (m *monitorSection) applyState() {
	m.startBtn.Disable()
	m.stopBtn.Disable()
	if !m.active {
		m.startBtn.Enable()
	} else {
		m.stopBtn.Enable()
	}
	if m.active {
		m.statusLabel.SetText("已监听")
	} else {
		m.statusLabel.SetText("未监听")
	}
}

func (m *monitorSection) invalidate() {
	m.active = false
	m.applyState()
	m.logView.Display("")
}

type MonitorPage struct {
	app      *App
	syscall  *monitorSection
	cntvct   *monitorSection
	content  fyne.CanvasObject
}

func newMonitorPage(a *App) *MonitorPage {
	p := &MonitorPage{app: a}
	p.syscall = &monitorSection{app: a, op: "syscall"}
	p.cntvct = &monitorSection{app: a, op: "cntvct"}
	syscallCard := widget.NewCard("系统调用监听（lsdriver）", "", p.syscall.build())
	cntvctCard := widget.NewCard("CNTVCT_EL0 读取监听", "", p.cntvct.build())
	p.content = container.NewBorder(
		nil, nil, nil, nil,
		container.NewVBox(syscallCard, cntvctCard),
	)
	return p
}

func (p *MonitorPage) Content() fyne.CanvasObject { return p.content }

func (p *MonitorPage) invalidate() {
	p.syscall.invalidate()
	p.cntvct.invalidate()
}

func (m *monitorSection) onStart() {
	a := m.app
	a.runOp(m.op+".start", nil, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "监听失败", "") {
			return
		}
		data := respData(resp)
		m.active = true
		m.applyState()
		pid := toInt64(data["pid"])
		a.setStatus(fmtMonitorStart(m.op, pid))
		m.onRefresh()
	})
}

func fmtMonitorStart(op string, pid int64) string {
	if op == "syscall" {
		return "已监听 PID " + itoa(pid) + " 的系统调用"
	}
	return "已开始监听 CNTVCT_EL0 读取（PID " + itoa(pid) + "）"
}

func itoa(v int64) string {
	if v == 0 {
		return "0"
	}
	negative := v < 0
	if negative {
		v = -v
	}
	var digits []byte
	for v > 0 {
		digits = append([]byte{byte('0' + v%10)}, digits...)
		v /= 10
	}
	if negative {
		return "-" + string(digits)
	}
	return string(digits)
}

func (m *monitorSection) onStop() {
	a := m.app
	a.runOp(m.op+".stop", nil, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "停止失败", "") {
			return
		}
		m.active = false
		m.applyState()
		if m.op == "syscall" {
			a.setStatus("系统调用监听已停止")
		} else {
			a.setStatus("CNTVCT_EL0 监听已停止")
		}
	})
}

func (m *monitorSection) onRefresh() {
	a := m.app
	a.runOp(m.op+".read", nil, func(resp *bridge.Response) {
		if !respOK(resp) {
			return
		}
		data := respData(resp)
		m.logView.Display(strings.TrimRight(toString(data["log"]), "\n"))
		if m.op == "syscall" {
			a.setStatus("系统调用日志已刷新，共 " + itoa(toInt64(data["line_count"])) + " 行")
		}
	})
}
