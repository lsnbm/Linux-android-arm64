package ui

import (
	"fmt"
	"strings"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/widget"

	"luckystar/internal/bridge"
)

type SigPage struct {
	app *App

	addrInput   *widget.Entry
	rangeInput  *widget.Entry
	fileInput   *widget.Entry
	scanBtn     *widget.Button
	filterInput *widget.Entry
	filterBtn   *widget.Button
	patternInput *widget.Entry
	offsetInput *widget.Entry
	patternBtn  *widget.Button
	statusLabel *widget.Label
	view        *TextArea
	content     fyne.CanvasObject
}

func newSigPage(a *App) *SigPage {
	p := &SigPage{app: a}
	p.addrInput = widget.NewEntry()
	p.addrInput.SetText("0x0")
	p.addrInput.SetPlaceHolder("扫描并保存时使用")
	p.rangeInput = widget.NewEntry()
	p.rangeInput.SetText("50")
	p.fileInput = widget.NewEntry()
	p.fileInput.SetText("Signature.txt")
	p.scanBtn = widget.NewButton("找特征", p.onScanAddress)

	p.filterInput = widget.NewEntry()
	p.filterInput.SetText("0x0")
	p.filterInput.SetPlaceHolder("过滤 Signature.txt")
	p.filterBtn = widget.NewButton("过滤特征", p.onFilter)

	p.patternInput = widget.NewEntry()
	p.patternInput.SetPlaceHolder("例如 A1h ?? FFh 00h")
	p.offsetInput = widget.NewEntry()
	p.offsetInput.SetText("0")
	p.patternBtn = widget.NewButton("扫特征", p.onScanPattern)

	p.statusLabel = widget.NewLabel("特征码状态: 未执行")
	p.view = NewTextArea()
	p.view.SetReadOnly(true)

	row1 := container.NewHBox(widget.NewLabel("目标地址"), fixWidth(p.addrInput, 240),
		widget.NewLabel("范围"), fixWidth(p.rangeInput, 100),
		widget.NewLabel("文件"), fixWidth(p.fileInput, 200),
		p.scanBtn)
	row2 := container.NewHBox(widget.NewLabel("过滤地址"), fixWidth(p.filterInput, 300), p.filterBtn)
	row3 := container.NewHBox(widget.NewLabel("特征码"), fixWidth(p.patternInput, 360),
		widget.NewLabel("偏移"), fixWidth(p.offsetInput, 100),
		p.patternBtn)

	actions := widget.NewCard("扫描与过滤", "", container.NewVBox(row1, row2, row3))
	result := widget.NewCard("结果", "",
		container.NewBorder(container.NewVBox(p.statusLabel), nil, nil, nil, p.view))
	p.content = container.NewBorder(actions, nil, nil, nil, result)
	return p
}

func (p *SigPage) Content() fyne.CanvasObject { return p.content }

func (p *SigPage) invalidate() {
	p.statusLabel.SetText("特征码状态: 未执行")
	p.view.Display("")
}

func (p *SigPage) fileName() string {
	if name := strings.TrimSpace(p.fileInput.Text); name != "" {
		return name
	}
	return "Signature.txt"
}

func (p *SigPage) onScanAddress() {
	a := p.app
	addr, err := parseBase0(p.addrInput.Text)
	if err != nil || addr < 0 {
		a.warn("输入提示", "目标地址格式无效。")
		return
	}
	scanRange, err := parseBase0(p.rangeInput.Text)
	if err != nil || scanRange < 1 || scanRange > 1200 {
		a.warn("输入提示", "范围必须在 1..1200 之间。")
		return
	}
	a.runOp("signature.create", map[string]any{
		"address":   fmt.Sprintf("0x%X", addr),
		"range":     scanRange,
		"file_name": p.fileName(),
	}, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "特征码失败", "") {
			p.statusLabel.SetText("特征码状态: 扫描并保存失败")
			return
		}
		data := respData(resp)
		p.statusLabel.SetText("特征码状态: 扫描并保存成功")
		p.view.Display(formatSigResult(data))
		a.setStatus("特征码扫描并保存成功")
	})
}

func (p *SigPage) onFilter() {
	a := p.app
	addr, err := parseBase0(p.filterInput.Text)
	if err != nil || addr < 0 {
		a.warn("输入提示", "过滤地址格式无效。")
		return
	}
	a.runOp("signature.filter", map[string]any{
		"address":   fmt.Sprintf("0x%X", addr),
		"file_name": p.fileName(),
	}, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "特征码失败", "") {
			p.statusLabel.SetText("特征码状态: 过滤失败")
			return
		}
		filterData := respData(resp)
		// merge then scan
		a.runOp("signature.scan", map[string]any{"file_name": p.fileName()}, func(scanResp *bridge.Response) {
			if !a.notifyIfOpFailed(scanResp, "特征码失败", "") {
				p.statusLabel.SetText("特征码状态: 过滤后扫描失败")
				return
			}
			scanData := respData(scanResp)
			merged := map[string]any{}
			for key, value := range scanData {
				merged[key] = value
			}
			if v, ok := filterData["changed_count"]; ok {
				merged["changed_count"] = v
			}
			if v, ok := filterData["total_count"]; ok {
				merged["total_count"] = v
			}
			if v, ok := filterData["old_signature"]; ok {
				merged["old_signature"] = v
			}
			if v, ok := filterData["new_signature"]; ok {
				merged["new_signature"] = v
			}
			if v, ok := filterData["file"]; ok {
				merged["file"] = v
			}
			p.statusLabel.SetText("特征码状态: 过滤成功")
			p.view.Display(formatSigResult(merged))
			a.setStatus("特征码过滤成功")
		})
	})
}

func (p *SigPage) onScanPattern() {
	a := p.app
	pattern := strings.TrimSpace(p.patternInput.Text)
	if pattern == "" {
		a.warn("输入提示", "请输入特征码。")
		return
	}
	rangeOffset, err := parseBase0(p.offsetInput.Text)
	if err != nil {
		a.warn("输入提示", "偏移格式无效。")
		return
	}
	a.runOp("signature.match", map[string]any{
		"pattern":      pattern,
		"range_offset": rangeOffset,
	}, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "特征码失败", "") {
			p.statusLabel.SetText("特征码状态: 按特征码扫描失败")
			return
		}
		p.statusLabel.SetText("特征码状态: 按特征码扫描完成")
		p.view.Display(formatSigResult(respData(resp)))
		a.setStatus("按特征码扫描完成")
	})
}
