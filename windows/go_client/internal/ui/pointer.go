package ui

import (
	"fmt"
	"strings"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/widget"

	"luckystar/internal/bridge"
)

type PointerPage struct {
	app *App

	targetInput  *widget.Entry
	depthInput   *widget.Entry
	offsetInput  *widget.Entry
	modeGroup    *widget.RadioGroup
	filterInput  *widget.Entry
	manualInput  *widget.Entry
	arrayBase    *widget.Entry
	arrayCount   *widget.Entry
	scanBtn      *widget.Button
	statusBtn    *widget.Button
	mergeBtn     *widget.Button
	exportBtn    *widget.Button
	statusLabel  *widget.Label
	view         *TextArea
	content      fyne.CanvasObject

	running bool
}

func newPointerPage(a *App) *PointerPage {
	p := &PointerPage{app: a}
	p.targetInput = widget.NewEntry()
	p.targetInput.SetText("0x0")
	p.targetInput.SetPlaceHolder("例如 0x12345678")
	p.depthInput = widget.NewEntry()
	p.depthInput.SetText("5")
	p.depthInput.SetPlaceHolder("1..16")
	p.offsetInput = widget.NewEntry()
	p.offsetInput.SetText("4096")
	p.offsetInput.SetPlaceHolder("> 0")

	p.modeGroup = widget.NewRadioGroup([]string{"Module", "Manual", "Array"}, func(string) { p.updateModeInputs() })

	p.filterInput = widget.NewEntry()
	p.filterInput.SetPlaceHolder("可选，例如 libil2cpp.so")
	p.manualInput = widget.NewEntry()
	p.manualInput.SetText("0x0")
	p.arrayBase = widget.NewEntry()
	p.arrayBase.SetText("0x0")
	p.arrayCount = widget.NewEntry()
	p.arrayCount.SetText("128")

	p.scanBtn = widget.NewButton("开始扫描", p.onScan)
	p.statusBtn = widget.NewButton("刷新状态", p.onStatus)
	p.mergeBtn = widget.NewButton("合并Bin", p.onMerge)
	p.exportBtn = widget.NewButton("导出文本", p.onExport)
	p.statusLabel = widget.NewLabel("扫描状态: 未开始")
	p.view = NewTextArea()
	p.view.SetReadOnly(true)

	row1 := container.NewHBox(widget.NewLabel("目标地址"), fixWidth(p.targetInput, 320),
		widget.NewLabel("深度"), fixWidth(p.depthInput, 90),
		widget.NewLabel("最大偏移"), fixWidth(p.offsetInput, 120))
	row2 := container.NewHBox(p.modeGroup, widget.NewLabel("模块过滤"), fixWidth(p.filterInput, 320))
	row3 := container.NewHBox(widget.NewLabel("手动基址"), fixWidth(p.manualInput, 260))
	row4 := container.NewHBox(widget.NewLabel("数组基址"), fixWidth(p.arrayBase, 260),
		widget.NewLabel("数组数量"), fixWidth(p.arrayCount, 110))
	row5 := container.NewHBox(p.scanBtn, p.statusBtn, p.mergeBtn, p.exportBtn)

	config := widget.NewCard("扫描配置", "",
		container.NewVBox(row1, row2, row3, row4, row5))
	result := widget.NewCard("扫描结果", "",
		container.NewBorder(container.NewVBox(p.statusLabel), nil, nil, nil, p.view))
	p.content = container.NewBorder(
		config,
		nil, nil, nil,
		result,
	)
	p.modeGroup.SetSelected("Module")
	p.updateModeInputs()
	return p
}

func (p *PointerPage) Content() fyne.CanvasObject { return p.content }

func (p *PointerPage) invalidate() {
	p.running = false
	p.applyControlState()
	p.statusLabel.SetText("扫描状态: 未开始")
	p.view.Display("")
}

func (p *PointerPage) currentMode() string {
	switch p.modeGroup.Selected {
	case "Manual":
		return "manual"
	case "Array":
		return "array"
	}
	return "module"
}

func (p *PointerPage) updateModeInputs() {
	mode := p.currentMode()
	disabled := mode != "module"
	setDisabled(p.filterInput, disabled)
	setDisabled(p.manualInput, mode != "manual")
	setDisabled(p.arrayBase, mode != "array")
	setDisabled(p.arrayCount, mode != "array")
}

func (p *PointerPage) applyControlState() {
	p.scanBtn.Disable()
	if !p.running {
		p.scanBtn.Enable()
	}
	p.mergeBtn.Disable()
	p.exportBtn.Disable()
	if !p.running {
		p.mergeBtn.Enable()
		p.exportBtn.Enable()
	}
}

func (p *PointerPage) onScan() {
	a := p.app
	target, err := parseBase0(p.targetInput.Text)
	if err != nil || target <= 0 {
		a.warn("输入提示", "目标地址必须是大于 0 的十六进制地址。")
		return
	}
	depth, err := parseBase0(p.depthInput.Text)
	if err != nil || depth < 1 || depth > 16 {
		a.warn("输入提示", "深度必须在 1..16 之间。")
		return
	}
	maxOffset, err := parseBase0(p.offsetInput.Text)
	if err != nil || maxOffset <= 0 {
		a.warn("输入提示", "最大偏移必须大于 0。")
		return
	}
	mode := p.currentMode()
	params := map[string]any{
		"mode":       mode,
		"target":     fmt.Sprintf("0x%X", target),
		"depth":      depth,
		"max_offset": maxOffset,
	}
	switch mode {
	case "manual":
		base, err := parseBase0(p.manualInput.Text)
		if err != nil || base <= 0 {
			a.warn("输入提示", "手动基址必须是大于 0 的十六进制地址。")
			return
		}
		params["manual_base"] = fmt.Sprintf("0x%X", base)
	case "array":
		base, err := parseBase0(p.arrayBase.Text)
		if err != nil || base <= 0 {
			a.warn("输入提示", "数组基址必须是大于 0 的十六进制地址。")
			return
		}
		count, err := parseBase0(p.arrayCount.Text)
		if err != nil || count < 1 || count > 1_000_000 {
			a.warn("输入提示", "数组数量必须在 1..1000000 之间。")
			return
		}
		params["array_base"] = fmt.Sprintf("0x%X", base)
		params["array_count"] = count
	}
	if filter := strings.TrimSpace(p.filterInput.Text); filter != "" {
		params["module_filter"] = filter
	}

	a.runOp("pointer.scan", params, func(resp *bridge.Response) {
		p.view.Display(p.view.Text + "\n启动操作: pointer.scan " + compactJSON(params))
		if !a.notifyIfOpFailed(resp, "指针扫描失败", "指针扫描启动失败: ") {
			a.setStatus("指针扫描启动失败")
			return
		}
		p.running = true
		p.applyControlState()
		a.setStatus("指针扫描已启动")
		a.live.TickNow()
	})
}

func (p *PointerPage) onStatus() {
	a := p.app
	a.runOp("pointer.get", nil, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "指针扫描失败", "") {
			return
		}
		p.applyStatusData(respData(resp), false)
	})
}

func (p *PointerPage) onMerge() {
	a := p.app
	a.runOp("pointer.merge", nil, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "合并失败", "") {
			return
		}
		p.running = true
		p.applyControlState()
		p.view.Display(p.view.Text + "\n已触发 Pointer.bin 合并任务。")
		a.live.TickNow()
	})
}

func (p *PointerPage) onExport() {
	a := p.app
	a.runOp("pointer.export", nil, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "导出失败", "") {
			return
		}
		p.running = true
		p.applyControlState()
		p.view.Display(p.view.Text + "\n已触发指针链文本导出。")
		a.live.TickNow()
	})
}

func (p *PointerPage) applyStatusData(data map[string]any, silent bool) {
	a := p.app
	busy := toBool(data["busy"])
	operation := toString(data["operation"])
	completed := toBool(data["completed"])
	success := toBool(data["success"])
	progress := toInt64(data["progress"])
	count := toInt64(data["count"])
	errorText := toString(data["error"])

	p.running = busy
	p.applyControlState()

	switch {
	case busy:
		p.statusLabel.SetText(fmt.Sprintf("指针操作: %s, progress=%d, count=%d", operation, progress, count))
	case completed && success:
		p.statusLabel.SetText(fmt.Sprintf("指针操作完成: count=%d", count))
	case completed && !success:
		if errorText == "" {
			errorText = "未知错误"
		}
		p.statusLabel.SetText("指针操作失败: " + errorText)
	default:
		p.statusLabel.SetText("指针状态: 未开始")
	}
	if !silent {
		p.view.Display(p.view.Text + "\n" + p.statusLabel.Text)
		a.setStatus(p.statusLabel.Text)
	}
}

func setDisabled(w *widget.Entry, disabled bool) {
	if disabled {
		w.Disable()
	} else {
		w.Enable()
	}
}
