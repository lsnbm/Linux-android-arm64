package ui

import (
	"fmt"
	"regexp"
	"strconv"
	"strings"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/widget"

	"luckystar/internal/bridge"
)

var valueTypeOptions = []string{"I8", "I16", "I32", "I64", "Float", "Double"}

var valueTypeTokens = map[string]string{
	"I8": "i8", "I16": "i16", "I32": "i32", "I64": "i64", "Float": "f32", "Double": "f64",
}

var scanModeOptions = []string{
	"未知", "等于", "大于", "小于", "增加", "减少", "已变化", "未变化", "范围", "指针", "字符串",
}

var scanModeTokens = map[string]string{
	"未知": "unknown", "等于": "equal", "大于": "greater", "小于": "less",
	"增加": "increased", "减少": "decreased", "已变化": "changed", "未变化": "unchanged",
	"范围": "range", "指针": "pointer", "字符串": "string",
}

var scanTypeAliases = map[string]string{
	"i8": "i8", "int8": "i8", "i16": "i16", "int16": "i16",
	"i32": "i32", "int32": "i32", "i64": "i64", "int64": "i64",
	"float": "f32", "f32": "f32", "double": "f64", "f64": "f64",
	"str": "string", "string": "string", "text": "string",
}

type ScanPage struct {
	app *App

	view       *TextArea
	typeCombo  *widget.Select
	modeCombo  *widget.Select
	valueInput *widget.Entry
	rangeInput *widget.Entry

	firstBtn     *widget.Button
	nextBtn      *widget.Button
	statusBtn    *widget.Button
	clearBtn     *widget.Button
	prevBtn      *widget.Button
	nextPageBtn  *widget.Button
	pageCount    *widget.Entry
	totalLabel   *widget.Label
	content      fyne.CanvasObject

	sessionType string
	liveRefresh bool
	pageStart   int64
	totalCount  int64
}

func newScanPage(a *App) *ScanPage {
	p := &ScanPage{app: a}

	p.view = NewTextArea()
	p.view.SetReadOnly(true)
	p.view.SetPlaceHolder("首次扫描后显示结果；右键可保存到保存页。")

	p.typeCombo = widget.NewSelect(valueTypeOptions, func(string) { p.applyControlState() })
	p.modeCombo = widget.NewSelect(scanModeOptions, func(string) { p.applyControlState() })

	p.valueInput = widget.NewEntry()
	p.valueInput.SetPlaceHolder("例如 100 或 3.14")
	p.rangeInput = widget.NewEntry()
	p.rangeInput.SetText("0")
	p.rangeInput.SetPlaceHolder("range 模式使用")

	p.firstBtn = widget.NewButton("首次扫描", func() { p.runScan(true) })
	p.nextBtn = widget.NewButton("再次扫描", func() { p.runScan(false) })
	p.statusBtn = widget.NewButton("扫描状态", p.onStatus)
	p.clearBtn = widget.NewButton("清空结果", p.onClear)
	p.prevBtn = widget.NewButton("上一页", func() { p.onPage(-1) })
	p.nextPageBtn = widget.NewButton("下一页", func() { p.onPage(1) })
	p.pageCount = widget.NewEntry()
	p.pageCount.SetText("100")
	p.pageCount.SetPlaceHolder("1..200")
	p.totalLabel = widget.NewLabel("总结果数: 0")

	p.view.SetMenu(func(pos fyne.Position) { p.showContextMenu(pos) })

	paramsCard := container.NewVBox(
		container.NewHBox(widget.NewLabel("类型"), p.typeCombo, widget.NewLabel("模式"), p.modeCombo),
		container.NewHBox(widget.NewLabel("值"), fixWidth(p.valueInput, 300), widget.NewLabel("范围"), fixWidth(p.rangeInput, 110)),
		container.NewHBox(p.firstBtn, p.nextBtn, p.statusBtn, p.clearBtn),
		container.NewHBox(p.totalLabel),
		container.NewHBox(widget.NewLabel("分页数量"), fixWidth(p.pageCount, 110), p.prevBtn, p.nextPageBtn),
	)
	paramsCard = container.NewVBox(widget.NewLabelWithStyle("扫描参数", fyne.TextAlignLeading, fyne.TextStyle{Bold: true}), paramsCard)

	split := container.NewHSplit(p.view, paramsCard)
	split.SetOffset(0.62)
	p.content = widget.NewCard("内存扫描", "", split)
	p.typeCombo.SetSelected("I32")
	p.modeCombo.SetSelected("等于")
	p.applyControlState()
	return p
}

func (p *ScanPage) Content() fyne.CanvasObject { return p.content }

func (p *ScanPage) invalidate() {
	p.sessionType = ""
	p.liveRefresh = false
	p.pageStart = 0
	p.totalCount = 0
	p.view.Display("")
	p.totalLabel.SetText("总结果数: 0")
	p.applyControlState()
}

// ---- state ----

func (p *ScanPage) modeToken() string {
	label := p.modeCombo.Selected
	return scanModeTokens[label]
}

func (p *ScanPage) selectedTypeToken() string {
	return valueTypeTokens[p.typeCombo.Selected]
}

func (p *ScanPage) resultTypeToken() string {
	if p.sessionType != "" {
		return p.sessionType
	}
	return p.selectedTypeToken()
}

func (p *ScanPage) applyControlState() {
	hasBaseline := p.sessionType != ""
	running := p.liveRefresh
	mode := p.modeToken()

	if !hasBaseline && bridge.ScanHistoryModes[mode] {
		p.modeCombo.SetSelected("等于")
		mode = p.modeToken()
	}
	if hasBaseline && mode == "unknown" {
		p.modeCombo.SetSelected("等于")
		mode = p.modeToken()
	}
	if p.sessionType == "string" && mode != "string" {
		p.modeCombo.SetSelected("字符串")
		mode = p.modeToken()
	}
	if mode == "pointer" {
		p.typeCombo.SetSelected("I64")
	}

	p.typeCombo.Disable() // re-enabled below when allowed
	if !running && !hasBaseline && mode != "pointer" && mode != "string" {
		p.typeCombo.Enable()
	}
	p.modeCombo.Disable()
	if !running && p.sessionType != "string" {
		p.modeCombo.Enable()
	}
	p.valueInput.Disable()
	if !running && bridge.ScanValueModes[mode] {
		p.valueInput.Enable()
	}
	p.rangeInput.Disable()
	if !running && mode == "range" {
		p.rangeInput.Enable()
	}
	p.firstBtn.Disable()
	if !running && !hasBaseline && !bridge.ScanHistoryModes[mode] {
		p.firstBtn.Enable()
	}
	p.nextBtn.Disable()
	if !running && hasBaseline {
		p.nextBtn.Enable()
	}
	p.clearBtn.Disable()
	if !running && hasBaseline {
		p.clearBtn.Enable()
	}
	p.prevBtn.Disable()
	if !running && hasBaseline {
		p.prevBtn.Enable()
	}
	p.nextPageBtn.Disable()
	if !running && hasBaseline {
		p.nextPageBtn.Enable()
	}

	switch mode {
	case "string":
		p.valueInput.SetPlaceHolder("输入要扫描的文本")
	case "pointer":
		p.valueInput.SetPlaceHolder("输入十六进制目标地址，例如 7F12345678")
	default:
		p.valueInput.SetPlaceHolder("例如 100 或 3.14")
	}
}

// adoptState mirrors _adopt_scan_state.
func (p *ScanPage) adoptState(data map[string]any, fallbackType, fallbackMode string) {
	stateType := scanTypeAliases[strings.ToLower(strings.TrimSpace(toString(data["value_type"])))]
	if stateType == "" && toBool(data["string_scan"]) {
		stateType = "string"
	}
	if stateType == "" {
		stateType = scanTypeAliases[strings.ToLower(strings.TrimSpace(fallbackType))]
	}
	p.sessionType = stateType
	if stateType != "" && stateType != "string" {
		for label, token := range valueTypeTokens {
			if token == stateType {
				p.typeCombo.SetSelected(label)
				break
			}
		}
	}
	stateMode := strings.ToLower(strings.TrimSpace(toString(data["mode"])))
	if stateMode == "" {
		stateMode = fallbackMode
	}
	if stateType == "string" {
		stateMode = "string"
	}
	if stateMode != "" {
		for label, token := range scanModeTokens {
			if token == stateMode {
				p.modeCombo.SetSelected(label)
				break
			}
		}
	}
	p.applyControlState()
}

// ---- actions ----

func (p *ScanPage) runScan(isFirst bool) {
	a := p.app
	mode := p.modeToken()
	value := strings.TrimSpace(p.valueInput.Text)
	rangeText := strings.TrimSpace(p.rangeInput.Text)

	if !isFirst && p.sessionType == "" {
		a.warn("输入提示", "请先完成首次扫描。")
		return
	}
	valueType := p.sessionType
	if valueType == "" {
		valueType = p.selectedTypeToken()
	}
	params, err := bridge.BuildScanParams(valueType, mode, value, rangeText, isFirst)
	if err != nil {
		a.warn("输入提示", err.Error())
		return
	}
	operation := "scan.start"
	if !isFirst {
		operation = "scan.refine"
	}
	label := "首次"
	if !isFirst {
		label = "再次"
	}
	a.runOp(operation, params, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "扫描失败", label+"扫描失败: ") {
			a.setStatus(label + "扫描失败")
			return
		}
		data := respData(resp)
		p.adoptState(data, "string", params["mode"].(string))
		p.pageStart = 0
		p.totalCount = 0
		p.totalLabel.SetText("总结果数: 0")
		p.liveRefresh = true
		p.applyControlState()
		a.setStatus(label + "扫描已启动")
		a.live.TickNow()
	})
}

func (p *ScanPage) onStatus() {
	a := p.app
	a.runOp("scan.get", nil, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "扫描失败", "") {
			return
		}
		data := respData(resp)
		scanning := toBool(data["scanning"])
		p.liveRefresh = scanning
		p.adoptState(data, p.resultTypeToken(), p.modeToken())
		p.applyControlState()
		a.setStatus(fmt.Sprintf("扫描状态: scanning=%v, progress=%d, count=%d",
			map[bool]int{true: 1, false: 0}[scanning], toInt64(data["progress"]), toInt64(data["count"])))
	})
}

func (p *ScanPage) onClear() {
	a := p.app
	a.runOp("scan.clear", nil, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "扫描失败", "") {
			return
		}
		p.view.Display("")
		p.sessionType = ""
		p.liveRefresh = false
		p.pageStart = 0
		p.totalCount = 0
		p.totalLabel.SetText("总结果数: 0")
		p.applyControlState()
		a.setStatus("扫描结果已清空")
	})
}

func (p *ScanPage) pageSize() (int64, bool) {
	size, err := strconv.ParseInt(strings.TrimSpace(p.pageCount.Text), 10, 64)
	if err != nil || size < 1 || size > 200 {
		return 0, false
	}
	return size, true
}

func (p *ScanPage) onPage(direction int) {
	a := p.app
	size, ok := p.pageSize()
	if !ok {
		a.warn("输入提示", "分页数量必须是 1..200 之间的整数。")
		return
	}
	start := p.pageStart
	if direction < 0 {
		start = p.pageStart - size
		if start < 0 {
			start = 0
		}
	} else {
		if p.totalCount > 0 && p.pageStart+size >= p.totalCount {
			a.setStatus("已经是最后一页")
			return
		}
		start = p.pageStart + size
	}
	p.fetchPage(start, false)
}

func (p *ScanPage) fetchPage(start int64, silent bool) {
	a := p.app
	size, ok := p.pageSize()
	if !ok {
		if !silent {
			a.warn("输入提示", "分页数量必须是 1..200 之间的整数。")
		}
		return
	}
	a.runOp("scan.results", map[string]any{
		"start":      start,
		"count":      size,
		"value_type": p.resultTypeToken(),
	}, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "扫描失败", "") {
			return
		}
		data := respData(resp)
		p.renderPage(data)
		p.pageStart = toInt64(data["start"])
		p.totalCount = toInt64(data["total_count"])
		p.totalLabel.SetText(fmt.Sprintf("总结果数: %d", p.totalCount))
		p.liveRefresh = false
		p.applyControlState()
		if !silent {
			a.setStatus(fmt.Sprintf("扫描结果已刷新：start=%d, total=%d", p.pageStart, p.totalCount))
		}
	})
}

func (p *ScanPage) renderPage(data map[string]any) {
	start := toInt64(data["start"])
	items, _ := data["items"].([]any)
	var sb strings.Builder
	if len(items) == 0 {
		sb.WriteString("本页没有结果。\n")
	} else {
		for idx, raw := range items {
			if item, ok := raw.(map[string]any); ok {
				fmt.Fprintf(&sb, "%08d | %-18s | %s\n", start+int64(idx),
					toString(item["addr_hex"]), toString(item["value"]))
			} else {
				fmt.Fprintf(&sb, "%08d | 非法数据\n", start+int64(idx))
			}
		}
	}
	p.view.Display(strings.TrimRight(sb.String(), "\n"))
}

// ---- context menu ----

var scanLinePattern = regexp.MustCompile(`^\s*\d+\s*\|\s*(0x[0-9A-Fa-f]+)\s*\|\s*(.*)$`)

func (p *ScanPage) showContextMenu(pos fyne.Position) {
	a := p.app
	lines := p.currentLines()
	var addrs []string
	for _, line := range lines {
		if match := scanLinePattern.FindStringSubmatch(line); match != nil {
			addrs = append(addrs, match[1])
		}
	}
	if len(addrs) == 0 {
		return
	}
	valueKind := "numeric"
	if p.modeToken() == "pointer" {
		valueKind = "pointer"
	} else if p.resultTypeToken() == "string" {
		valueKind = "text"
	}
	menu := fyne.NewMenu("",
		fyne.NewMenuItem(fmt.Sprintf("保存到保存页 (%d 项)", len(addrs)), func() {
			p.saveScanAddrs(addrs, valueKind)
		}),
	)
	pop := widget.NewPopUpMenu(menu, a.Win.Canvas())
	pop.ShowAtPosition(pos)
}

func (p *ScanPage) currentLines() []string {
	if selected := strings.TrimSpace(p.view.SelectedText()); selected != "" {
		return strings.Split(selected, "\n")
	}
	text := p.view.Text
	if text == "" {
		return nil
	}
	all := strings.Split(text, "\n")
	row := p.view.CursorRow
	if row < 0 || row >= len(all) {
		return nil
	}
	return []string{all[row]}
}

func (p *ScanPage) saveScanAddrs(addrs []string, valueKind string) {
	a := p.app
	typeToken := p.resultTypeToken()
	go func() {
		success, failed := 0, 0
		for _, addr := range addrs {
			params, err := bridge.BuildSavedAddParams(addr, typeToken, valueKind, 64, "")
			if err != nil {
				failed++
				continue
			}
			resp, err := a.Br.CallOperation("saved.add", params)
			if err == nil && resp.Ok {
				success++
			} else {
				failed++
			}
		}
		fyne.Do(func() {
			if failed > 0 {
				a.warn("保存失败", fmt.Sprintf("成功 %d 项，失败 %d 项", success, failed))
			}
			a.setStatus(fmt.Sprintf("已保存 %d 项，失败 %d 项", success, failed))
			a.savePage.refreshState(true)
		})
	}()
}

// liveRefreshSnapshot returns data for the background refresher.
func (p *ScanPage) liveRefreshSnapshot() *liveTask {
	if !p.liveRefresh {
		return nil
	}
	size, ok := p.pageSize()
	if !ok {
		size = 100
	}
	return &liveTask{kind: "scan", scanStart: p.pageStart, scanCount: size, scanValueType: p.resultTypeToken()}
}

// applyLiveRefreshScan handles the scan.get + scan.results live refresh.
func (p *ScanPage) applyLiveRefreshScan(getData map[string]any, pageData map[string]any) {
	a := p.app
	p.adoptState(getData, p.resultTypeToken(), p.modeToken())
	if toBool(getData["scanning"]) {
		return
	}
	if pageData != nil {
		p.renderPage(pageData)
		p.pageStart = toInt64(pageData["start"])
		p.totalCount = toInt64(pageData["total_count"])
		p.totalLabel.SetText(fmt.Sprintf("总结果数: %d", p.totalCount))
	}
	p.liveRefresh = false
	p.applyControlState()
	a.setStatus(fmt.Sprintf("扫描完成：共 %d 项", p.totalCount))
}
