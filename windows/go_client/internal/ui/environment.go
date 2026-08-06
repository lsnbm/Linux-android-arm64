package ui

import (
	"fmt"
	"strings"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/widget"

	"luckystar/internal/bridge"
)

type EnvPage struct {
	app *App

	threadInput *widget.Entry
	getBtn      *widget.Button
	view        *TextArea
	content     fyne.CanvasObject
}

func newEnvPage(a *App) *EnvPage {
	p := &EnvPage{app: a}
	p.threadInput = widget.NewEntry()
	p.threadInput.SetPlaceHolder("可选，task->comm，最多 15 字符")
	p.threadInput.OnSubmitted = func(string) { p.onGet() }
	p.getBtn = widget.NewButton("获取环境参数", p.onGet)
	p.view = NewTextArea()
	p.view.SetReadOnly(true)
	p.view.SetPlaceHolder("留空线程名可获取 PACGA；填写线程名时同时获取 TPIDR_EL0。")

	row := container.NewHBox(widget.NewLabel("线程名"), fixWidth(p.threadInput, 300), p.getBtn)
	p.content = widget.NewCard("环境参数", "",
		container.NewBorder(container.NewVBox(row), nil, nil, nil, p.view),
	)
	return p
}

func (p *EnvPage) Content() fyne.CanvasObject { return p.content }

func (p *EnvPage) invalidate() {
	p.view.Display("")
}

func (p *EnvPage) onGet() {
	a := p.app
	threadName := strings.TrimSpace(p.threadInput.Text)
	a.runOp("env.read", map[string]any{"thread_name": threadName}, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "获取失败", "获取环境参数失败: ") {
			return
		}
		data := respData(resp)
		var sb strings.Builder
		fmt.Fprintf(&sb, "PID: %d\n", toInt64(data["pid"]))
		threadDisplay := threadName
		if threadDisplay == "" {
			threadDisplay = "(未指定)"
		}
		fmt.Fprintf(&sb, "线程名: %s\n", threadDisplay)
		fmt.Fprintf(&sb, "TPIDR_EL0: %s\n", hexOrDefault(data["tpidr_el0"]))
		fmt.Fprintf(&sb, "PACGA_LO: %s\n", hexOrDefault(data["pacga_lo"]))
		fmt.Fprintf(&sb, "PACGA_HI: %s\n", hexOrDefault(data["pacga_hi"]))
		fmt.Fprintf(&sb, "TLS 状态: %s\n", toString(data["tls_status"]))
		fmt.Fprintf(&sb, "PACGA 状态: %s", toString(data["pacga_status"]))
		p.view.Display(sb.String())
		a.setStatus("环境参数获取成功")
	})
}

func hexOrDefault(v any) string {
	value := toInt64(v)
	if value == 0 && toString(v) == "" {
		return "0x0"
	}
	return fmt.Sprintf("0x%X", value)
}
