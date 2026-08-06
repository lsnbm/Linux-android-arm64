package ui

import (
	"fmt"
	"strings"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/widget"

	"luckystar/internal/bridge"
)

const browserWindowBytes = 100

var browserViewOptions = []string{
	"Hexadecimal", "Hex", "I8", "I16", "I32", "I64", "Float", "Double", "Disasm",
}

var browserViewTokens = map[string]string{
	"Hexadecimal": "hexadecimal", "Hex": "hex", "I8": "i8", "I16": "i16",
	"I32": "i32", "I64": "i64", "Float": "f32", "Double": "f64", "Disasm": "disasm",
}

type BrowserPage struct {
	app *App

	addrInput *widget.Entry
	sizeLabel *widget.Label
	viewCombo *widget.Select
	readBtn   *widget.Button
	refreshBtn *widget.Button
	upBtn     *widget.Button
	downBtn   *widget.Button
	view      *TextArea
	content   fyne.CanvasObject

	currentAddr int64
}

func newBrowserPage(a *App) *BrowserPage {
	p := &BrowserPage{app: a}
	p.addrInput = widget.NewEntry()
	p.addrInput.SetText("0x0")
	p.addrInput.SetPlaceHolder("输入起始地址，如 0x12345678 或 0x12345678+0xA8")
	p.addrInput.OnSubmitted = func(string) { p.onRead() }
	p.sizeLabel = widget.NewLabel(fmt.Sprintf("%d 字节", browserWindowBytes))
	p.viewCombo = widget.NewSelect(browserViewOptions, func(string) { p.onFormatChanged() })
	p.viewCombo.SetSelected("Hexadecimal")
	p.readBtn = widget.NewButton("读取", p.onRead)
	p.refreshBtn = widget.NewButton("刷新", p.onRefresh)
	p.upBtn = widget.NewButton("上移", func() { p.onSeek(-1) })
	p.downBtn = widget.NewButton("下移", func() { p.onSeek(1) })

	p.view = NewTextArea()
	p.view.SetReadOnly(true)

	row := container.NewHBox(
		widget.NewLabel("地址"),
		fixWidth(p.addrInput, 420),
		p.sizeLabel,
		widget.NewLabel("显示"),
		p.viewCombo,
		p.readBtn,
		p.refreshBtn,
		p.upBtn,
		p.downBtn,
	)
	p.content = widget.NewCard("内存浏览", "",
		container.NewBorder(container.NewVBox(row), nil, nil, nil, p.view),
	)
	return p
}

func (p *BrowserPage) Content() fyne.CanvasObject { return p.content }

func (p *BrowserPage) invalidate() {
	p.currentAddr = 0
	p.addrInput.SetText("0x0")
	p.view.Display("")
}

func (p *BrowserPage) viewMode() string {
	token := browserViewTokens[p.viewCombo.Selected]
	if token == "" {
		return "hexadecimal"
	}
	return token
}

func (p *BrowserPage) onRead() {
	a := p.app
	addrText := strings.TrimSpace(p.addrInput.Text)
	if addrText == "" {
		a.warn("输入提示", "请输入起始地址。")
		return
	}
	addr, err := parseAddressExpression(addrText)
	if err != nil {
		a.warn("输入提示", "地址格式无效："+err.Error())
		return
	}
	if addr < 0 {
		a.warn("输入提示", "地址不能为负数。")
		return
	}
	a.runOpQuiet("viewer.open", map[string]any{
		"address":    fmt.Sprintf("0x%X", addr),
		"view_format": p.viewMode(),
	}, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "读取失败", "打开浏览器失败: ") {
			return
		}
		snapshot := respData(resp)
		if !toBool(snapshot["read_success"]) {
			a.warn("读取失败", "MemViewer 读取失败。")
			return
		}
		p.applySnapshot(snapshot)
	})
}

func (p *BrowserPage) onRefresh() {
	a := p.app
	a.runOpQuiet("viewer.refresh", nil, func(resp *bridge.Response) {
		if !respOK(resp) {
			return
		}
		p.applySnapshot(respData(resp))
	})
}

func (p *BrowserPage) onFormatChanged() {
	a := p.app
	if !a.Br.IsConnected() || p.currentAddr <= 0 {
		return
	}
	a.runOpQuiet("viewer.format", map[string]any{"view_format": p.viewMode()}, func(resp *bridge.Response) {
		if !respOK(resp) {
			return
		}
		p.applySnapshot(respData(resp))
	})
}

func (p *BrowserPage) onSeek(direction int) {
	a := p.app
	if !a.Br.IsConnected() || p.currentAddr <= 0 {
		return
	}
	unit := scrollUnit(p.viewMode())
	bytes := int64(direction) * 8 * unit
	sign := "+"
	if bytes < 0 {
		sign = "-"
		bytes = -bytes
	}
	a.runOpQuiet("viewer.seek", map[string]any{"offset": fmt.Sprintf("%s0x%X", sign, bytes)}, func(resp *bridge.Response) {
		if !respOK(resp) {
			return
		}
		p.applySnapshot(respData(resp))
	})
}

func scrollUnit(mode string) int64 {
	switch mode {
	case "hexadecimal":
		return 8
	case "hex":
		return 16
	case "i8":
		return 1
	case "i16":
		return 2
	case "i32", "f32":
		return 4
	case "i64", "f64":
		return 8
	case "disasm":
		return 4
	}
	return 16
}

func (p *BrowserPage) applySnapshot(snapshot map[string]any) {
	base := toInt64(snapshot["base"])
	data := hexToBytes(toString(snapshot["data_hex"]))
	if data == nil {
		p.app.warn("读取失败", "MemViewer HEX 数据解析失败。")
		return
	}
	mode := p.viewMode()
	if mode == "disasm" {
		_, visible := extractDisasmWindow(snapshot)
		p.currentAddr = visible
		p.addrInput.SetText(fmt.Sprintf("0x%X", visible))
		p.view.Display(renderDisasmDump(snapshot))
		return
	}
	var text string
	switch mode {
	case "hex":
		text = renderHexDump(base, data)
	case "hexadecimal":
		text = renderHexadecimalDump(base, data)
	default:
		text = renderTypedDump(base, data, mode)
	}
	p.currentAddr = base
	p.addrInput.SetText(fmt.Sprintf("0x%X", base))
	p.view.Display(text)
}
