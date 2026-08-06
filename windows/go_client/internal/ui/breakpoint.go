package ui

import (
	"encoding/json"
	"fmt"
	"regexp"
	"strconv"
	"strings"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/widget"

	"luckystar/internal/bridge"
)

const hwbpMaxRegCount = 71

var hwbpRegIndex = map[string]int{
	"pc": 0, "hit_count": 1, "lr": 2, "sp": 3,
	"orig_x0": 4, "syscallno": 5, "pstate": 6,
	"fpsr": 37, "fpcr": 38,
}

const hwbpX0RegIndex = 7
const hwbpQ0RegIndex = 39

var hwbpPointTypeLabels = map[int]string{
	0: "未设置", 1: "读取", 2: "写入", 3: "读写", 4: "执行",
}

var hwbpScopeLabels = map[int]string{
	0: "主线程", 1: "子线程", 2: "全部",
}

var hwbpOpLabels = map[int]string{
	0: "未设置", 1: "读取", 2: "写入",
}

var bpTypeOptions = []string{"BP_READ", "BP_WRITE", "BP_READ_WRITE", "BP_EXECUTE"}
var bpTypeTokens = map[string]string{
	"BP_READ": "read", "BP_WRITE": "write", "BP_READ_WRITE": "read_write", "BP_EXECUTE": "execute",
}
var bpScopeOptions = []string{"BP_SCOPE_MAIN_THREAD", "BP_SCOPE_OTHER_THREADS", "BP_SCOPE_ALL_THREADS"}
var bpScopeTokens = map[string]string{
	"BP_SCOPE_MAIN_THREAD": "main", "BP_SCOPE_OTHER_THREADS": "other", "BP_SCOPE_ALL_THREADS": "all",
}

var bpLenOptions = []string{"1字节", "2字节", "3字节", "4字节", "5字节", "6字节", "7字节", "8字节"}

type bpPointRow struct {
	addr   *widget.Entry
	typ    *widget.Select
	scope  *widget.Select
	length *widget.Select
	remove *widget.Button
}

type BpPage struct {
	app *App

	brpsLabel    *widget.Label
	wrpsLabel    *widget.Label
	pointsLabel  *widget.Label
	rowsBox      *fyne.Container
	addPointBtn  *widget.Button
	removePointBtn *widget.Button
	refreshBtn   *widget.Button
	hwbpSetBtn   *widget.Button
	hwbpRmBtn    *widget.Button
	ptebpSetBtn  *widget.Button
	ptebpRmBtn   *widget.Button
	stepbpSetBtn *widget.Button
	stepbpRmBtn  *widget.Button

	tree      *MenuTree
	treeModel *TreeModel
	content   fyne.CanvasObject

	bpInfoData     map[string]any
	breakpointMode string
	selectedIndex  int

	pointRows    []*bpPointRow
	flatRecords  []flatRecord
	recordUIDMap map[string]int
	fieldUIDMap  map[string]string
	valueUIDMap  map[string]string
	pointUIDMap  map[string]int
}

type flatRecord struct {
	flatIndex int
	pointIdx  int
	recordIdx int
	record    map[string]any
}

type visiblePoint struct {
	index int
	point map[string]any
}

func newBpPage(a *App) *BpPage {
	p := &BpPage{app: a, selectedIndex: -1}
	p.brpsLabel = widget.NewLabel("bp_info.num_brps: 0")
	p.wrpsLabel = widget.NewLabel("bp_info.num_wrps: 0")
	p.pointsLabel = widget.NewLabel("bp_info.points: []")
	p.pointsLabel.Wrapping = fyne.TextWrapWord

	p.rowsBox = container.NewVBox()
	p.addPointBtn = widget.NewButton("添加point", p.addPointRow)
	p.removePointBtn = widget.NewButton("删除point", p.removeLastPointRow)
	p.refreshBtn = widget.NewButton("刷新断点信息", func() { p.refresh(false) })
	p.hwbpSetBtn = widget.NewButton("设置断点", func() { p.setMode("hwbp", "断点") })
	p.hwbpRmBtn = widget.NewButton("移除断点", func() { p.removeMode("hwbp", "断点") })
	p.ptebpSetBtn = widget.NewButton("设置PTEBP", func() { p.setMode("ptebp", "PTEBP") })
	p.ptebpRmBtn = widget.NewButton("移除PTEBP", func() { p.removeMode("ptebp", "PTEBP") })
	p.stepbpSetBtn = widget.NewButton("设置STEPBP", func() { p.setMode("stepbp", "STEPBP") })
	p.stepbpRmBtn = widget.NewButton("移除STEPBP", func() { p.removeMode("stepbp", "STEPBP") })

	model := NewTreeModel()
	p.treeModel = model
	p.tree = NewMenuTree(model)
	p.tree.SetSelectedChanged(func(uid widget.TreeNodeID) {
		if idx, ok := p.recordUIDMap[uid]; ok {
			p.selectedIndex = idx
		}
	})
	p.tree.onMenu = func(pos fyne.Position, uid widget.TreeNodeID) {
		p.showTreeMenu(pos, uid)
	}

	summary := container.NewVBox(
		container.NewHBox(p.brpsLabel, p.wrpsLabel),
		p.pointsLabel,
	)
	actions := container.NewHBox(
		p.addPointBtn, p.removePointBtn,
		widget.NewSeparator(),
		p.refreshBtn,
		p.hwbpSetBtn, p.hwbpRmBtn,
		p.ptebpSetBtn, p.ptebpRmBtn,
		p.stepbpSetBtn, p.stepbpRmBtn,
	)
	config := widget.NewCard("断点配置", "",
		container.NewVBox(summary, p.rowsBox, actions))
	result := widget.NewCard("断点树（hit_addr / records）", "", p.tree)
	p.content = container.NewBorder(config, nil, nil, nil, result)

	p.addPointRow()
	p.applyActiveState()
	return p
}

func (p *BpPage) Content() fyne.CanvasObject { return p.content }

func (p *BpPage) invalidate() {
	p.bpInfoData = nil
	p.breakpointMode = ""
	p.selectedIndex = -1
	p.brpsLabel.SetText("bp_info.num_brps: 0")
	p.wrpsLabel.SetText("bp_info.num_wrps: 0")
	p.pointsLabel.SetText("bp_info.points: []")
	p.tree.UnselectAll()
	p.clearTree()
	p.applyActiveState()
}

func (p *BpPage) clearTree() {
	p.recordUIDMap = map[string]int{}
	p.fieldUIDMap = map[string]string{}
	p.valueUIDMap = map[string]string{}
	p.pointUIDMap = map[string]int{}
	p.treeModel.Clear()
}

// ---- point rows ----

func (p *BpPage) addPointRow() {
	if len(p.pointRows) >= 16 {
		return
	}
	row := &bpPointRow{}
	row.addr = widget.NewEntry()
	row.addr.SetPlaceHolder("0x7A12345678")
	row.typ = widget.NewSelect(bpTypeOptions, nil)
	row.typ.SetSelected("BP_EXECUTE")
	row.scope = widget.NewSelect(bpScopeOptions, nil)
	row.scope.SetSelected("BP_SCOPE_ALL_THREADS")
	row.length = widget.NewSelect(bpLenOptions, nil)
	row.length.SetSelected("4字节")
	row.remove = widget.NewButton("删除", func() { p.removePointRow(row) })
	p.pointRows = append(p.pointRows, row)
	p.rebuildRows()
	p.applyActiveState()
}

func (p *BpPage) removeLastPointRow() {
	if len(p.pointRows) <= 1 {
		return
	}
	p.pointRows = p.pointRows[:len(p.pointRows)-1]
	p.rebuildRows()
	p.applyActiveState()
}

func (p *BpPage) removePointRow(target *bpPointRow) {
	if len(p.pointRows) <= 1 {
		return
	}
	for i, row := range p.pointRows {
		if row == target {
			p.pointRows = append(p.pointRows[:i], p.pointRows[i+1:]...)
			break
		}
	}
	p.rebuildRows()
	p.applyActiveState()
}

func (p *BpPage) rebuildRows() {
	var objects []fyne.CanvasObject
	for i, row := range p.pointRows {
		label := widget.NewLabel(fmt.Sprintf("P%d", i))
		hbox := container.NewHBox(label, fixWidth(row.addr, 280), row.typ, row.scope, row.length, row.remove)
		objects = append(objects, hbox)
	}
	p.rowsBox.Objects = objects
	p.rowsBox.Refresh()
}

func (p *BpPage) collectPoints() ([]map[string]any, error) {
	if len(p.pointRows) == 0 {
		return nil, fmt.Errorf("至少需要 1 个 point")
	}
	if len(p.pointRows) > 16 {
		return nil, fmt.Errorf("points 最多 16 个")
	}
	points := make([]map[string]any, 0, len(p.pointRows))
	for i, row := range p.pointRows {
		addrText := strings.TrimSpace(row.addr.Text)
		addr, err := strconv.ParseInt(addrText, 0, 64)
		if err != nil || addr <= 0 {
			return nil, fmt.Errorf("P%d 地址格式无效，必须为大于 0 的十六进制地址", i)
		}
		points = append(points, map[string]any{
			"address":  fmt.Sprintf("0x%X", addr),
			"bp_type":  bpTypeTokens[row.typ.Selected],
			"bp_scope": bpScopeTokens[row.scope.Selected],
			"length":   int64(strings.Index("12345678", string(row.length.Selected[0]))) + 1,
		})
	}
	return bridge.NormalizeBreakpointPoints(points)
}

// ---- mode set/remove ----

func (p *BpPage) setMode(mode, label string) {
	a := p.app
	if p.breakpointMode != "" {
		a.info("断点提示", "断点已激活，请先移除当前断点。")
		return
	}
	points, err := p.collectPoints()
	if err != nil {
		a.warn("输入提示", err.Error())
		return
	}
	a.runOp("breakpoint.set", map[string]any{"mode": mode, "points": points}, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "设置失败", "") {
			return
		}
		p.breakpointMode = mode
		p.applyActiveState()
		a.setStatus(fmt.Sprintf("设置 %s 成功: %d 个 points", label, len(points)))
		p.refresh(true)
	})
}

func (p *BpPage) removeMode(mode, label string) {
	a := p.app
	if p.breakpointMode != mode {
		a.setStatus(fmt.Sprintf("%s 未激活，无需移除", label))
		return
	}
	a.runOp("breakpoint.clear", nil, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "移除失败", "") {
			return
		}
		p.breakpointMode = ""
		p.applyActiveState()
		a.setStatus(fmt.Sprintf("已移除进程 %s", label))
		p.refresh(true)
	})
}

func (p *BpPage) refresh(silent bool) {
	a := p.app
	apply := func(resp *bridge.Response) {
		if !respOK(resp) {
			return
		}
		p.renderBPInfo(respData(resp))
	}
	if silent {
		a.runOpQuiet("breakpoint.get", nil, apply)
	} else {
		a.runOp("breakpoint.get", nil, func(resp *bridge.Response) {
			apply(resp)
			if respOK(resp) {
				a.setStatus("断点信息已刷新")
			}
		})
	}
}

func (p *BpPage) applyActiveState() {
	active := p.breakpointMode != ""
	p.hwbpSetBtn.Disable()
	p.ptebpSetBtn.Disable()
	p.stepbpSetBtn.Disable()
	if !active {
		p.hwbpSetBtn.Enable()
		p.ptebpSetBtn.Enable()
		p.stepbpSetBtn.Enable()
	}
	p.hwbpRmBtn.Disable()
	p.ptebpRmBtn.Disable()
	p.stepbpRmBtn.Disable()
	if p.breakpointMode == "hwbp" {
		p.hwbpRmBtn.Enable()
	}
	if p.breakpointMode == "ptebp" {
		p.ptebpRmBtn.Enable()
	}
	if p.breakpointMode == "stepbp" {
		p.stepbpRmBtn.Enable()
	}
	p.addPointBtn.Disable()
	p.removePointBtn.Disable()
	if !active {
		if len(p.pointRows) < 16 {
			p.addPointBtn.Enable()
		}
		if len(p.pointRows) > 1 {
			p.removePointBtn.Enable()
		}
	}
	for _, row := range p.pointRows {
		row.addr.Disable()
		row.typ.Disable()
		row.scope.Disable()
		row.length.Disable()
		row.remove.Disable()
		if !active {
			row.addr.Enable()
			row.typ.Enable()
			row.scope.Enable()
			row.length.Enable()
			if len(p.pointRows) > 1 {
				row.remove.Enable()
			}
		}
	}
}

// ---- rendering ----

func (p *BpPage) renderBPInfo(info map[string]any) {
	p.breakpointMode = strings.ToLower(strings.TrimSpace(toString(info["mode"])))
	bpInfo, _ := info["bp_info"].(map[string]any)
	if bpInfo == nil {
		bpInfo = map[string]any{}
	}
	numBrps := toInt64(bpInfo["num_brps"])
	numWrps := toInt64(bpInfo["num_wrps"])
	p.brpsLabel.SetText(fmt.Sprintf("bp_info.num_brps: %d", numBrps))
	p.wrpsLabel.SetText(fmt.Sprintf("bp_info.num_wrps: %d", numWrps))

	pointsRaw, _ := bpInfo["points"].([]any)
	var pointParts []string
	for pointIndex, raw := range pointsRaw {
		point, ok := raw.(map[string]any)
		if !ok {
			continue
		}
		hitAddr := toInt64(point["hit_addr"])
		if hitAddr <= 0 {
			continue
		}
		pointParts = append(pointParts, fmt.Sprintf("[%d] %s %s/%s/%s/records%d",
			pointIndex, formatAddrShort(hitAddr),
			hwbpPointTypeLabels[int(toInt64(point["bt"]))],
			hwbpScopeLabels[int(toInt64(point["bs"]))],
			pointLengthText(point),
			p.pointRecordCount(point)))
	}
	pointsText := "[]"
	if len(pointParts) > 0 {
		pointsText = strings.Join(pointParts, "; ")
	}
	modeLabel := "none"
	if p.breakpointMode != "" {
		modeLabel = strings.ToUpper(p.breakpointMode)
	}
	p.pointsLabel.SetText(fmt.Sprintf("bp_info.points: %s  mode: %s", pointsText, modeLabel))

	p.renderTree(bpInfo)
	p.applyActiveState()
}

func pointLengthText(point map[string]any) string {
	length := toInt64(point["bl"])
	if length > 0 {
		return fmt.Sprintf("%d字节", length)
	}
	return "未知"
}

func (p *BpPage) pointRecords(point map[string]any) []map[string]any {
	raw, _ := point["records"].([]any)
	var records []map[string]any
	for _, r := range raw {
		if m, ok := r.(map[string]any); ok {
			records = append(records, m)
		}
	}
	return records
}

func (p *BpPage) pointRecordCount(point map[string]any) int {
	records := p.pointRecords(point)
	count := toInt64(point["record_count"])
	if count < 0 {
		count = int64(len(records))
	}
	if count <= 0 && len(records) > 0 {
		count = int64(len(records))
	}
	if int(count) > len(records) {
		count = int64(len(records))
	}
	return int(count)
}

func (p *BpPage) renderTree(bpInfo map[string]any) {
	pointsRaw, _ := bpInfo["points"].([]any)

	// flat record index mapping
	pointFlatStarts := map[int]int{}
	flatIndex := 0
	for pointIndex, raw := range pointsRaw {
		point, ok := raw.(map[string]any)
		if !ok {
			continue
		}
		pointFlatStarts[pointIndex] = flatIndex
		flatIndex += p.pointRecordCount(point)
	}

	// visible points sorted by (hit_addr, point_index)
	var visible []visiblePoint
	for pointIndex, raw := range pointsRaw {
		point, ok := raw.(map[string]any)
		if !ok {
			continue
		}
		if toInt64(point["hit_addr"]) > 0 {
			visible = append(visible, visiblePoint{index: pointIndex, point: point})
		}
	}
	sortPoints(visible)

	roots := []string{}
	children := map[string][]string{}
	branch := map[string]bool{}
	texts := map[string]string{}
	recordUIDMap := map[string]int{}
	fieldUIDMap := map[string]string{}
	valueUIDMap := map[string]string{}
	pointUIDMap := map[string]int{}
	p.flatRecords = nil

	addBranch := func(uid string, parent *string) {
		branch[uid] = true
		if parent == nil {
			roots = append(roots, uid)
		} else {
			children[*parent] = append(children[*parent], uid)
		}
	}
	addLeaf := func(uid string, parent string, text string) {
		texts[uid] = text
		children[parent] = append(children[parent], uid)
	}

	if len(visible) == 0 {
		addLeaf("empty", "", "暂无 hit_addr 目录")
	} else {
		for _, vp := range visible {
			hitAddr := toInt64(vp.point["hit_addr"])
			records := p.pointRecords(vp.point)
			recordCount := p.pointRecordCount(vp.point)
			records = records[:minInt(recordCount, len(records))]
			totalHits := int64(0)
			for _, rec := range records {
				totalHits += toInt64(rec["hit_count"])
			}
			typeLabel := hwbpPointTypeLabels[int(toInt64(vp.point["bt"]))]
			scopeLabel := hwbpScopeLabels[int(toInt64(vp.point["bs"]))]
			lenText := pointLengthText(vp.point)
			pointUID := fmt.Sprintf("p%d", vp.index)
			pointUIDMap[pointUID] = vp.index
			addBranch(pointUID, nil)
			texts[pointUID] = fmt.Sprintf("0x%X  |  point[%d]  |  records %d  |  总命中 %d  |  %s/%s/%s",
				hitAddr, vp.index, recordCount, totalHits, typeLabel, scopeLabel, lenText)

			recordsDir := fmt.Sprintf("p%d.recs", vp.index)
			pointUIDMap[recordsDir] = vp.index
			addBranch(recordsDir, &pointUID)
			texts[recordsDir] = fmt.Sprintf("records  |  %d 条", recordCount)

			for localIdx, rec := range records {
				recordUID := fmt.Sprintf("p%d.r%d", vp.index, localIdx)
				flat := pointFlatStarts[vp.index] + localIdx
				recordUIDMap[recordUID] = flat
				pointUIDMap[recordUID] = vp.index
				p.flatRecords = append(p.flatRecords, flatRecord{
					flatIndex: flat, pointIdx: vp.index, recordIdx: localIdx, record: rec,
				})
				addBranch(recordUID, &recordsDir)

				pc := toInt64(rec["pc"])
				rwText := decodeHwbpRWText(rec)
				opsSummary := hwbpOpsSummary(rec)
				texts[recordUID] = fmt.Sprintf("PC 0x%X  |  point[%d:%d]  |  命中 %d 次  |  类型 %s  |  掩码 %s",
					pc, vp.index, localIdx, toInt64(rec["hit_count"]), rwText, opsSummary)

				baseFields := []struct{ name, label string }{
					{"pc", "PC"}, {"lr", "LR"}, {"sp", "SP"}, {"orig_x0", "ORIG_X0"},
					{"syscallno", "SYSCALLNO"}, {"pstate", "PSTATE"}, {"fpsr", "FPSR"}, {"fpcr", "FPCR"},
				}
				for _, bf := range baseFields {
					valueText := fmt.Sprintf("0x%X", toInt64(rec[bf.name]))
					uid := fmt.Sprintf("%s.f%s", recordUID, bf.name)
					recordUIDMap[uid] = flat
					fieldUIDMap[uid] = bf.name
					valueUIDMap[uid] = valueText
					addLeaf(uid, recordUID, fmt.Sprintf("  %s: %s  [%s]", bf.label, valueText, hwbpRegOp(rec, bf.name)))
				}

				if maskRaw, ok := rec["mask"].([]any); ok && len(maskRaw) > 0 {
					parts := make([]string, 0, 18)
					for i, rawByte := range maskRaw {
						if i >= 18 {
							break
						}
						parts = append(parts, fmt.Sprintf("%02X", toInt64(rawByte)&0xFF))
					}
					uid := recordUID + ".mask"
					recordUIDMap[uid] = flat
					addLeaf(uid, recordUID, "  MASK: "+strings.Join(parts, " "))
				}

				// X0~X29
				xregsUID := recordUID + ".xregs"
				recordUIDMap[xregsUID] = flat
				pointUIDMap[xregsUID] = vp.index
				addBranch(xregsUID, &recordUID)
				texts[xregsUID] = "  寄存器快照 X0~X29"
				for regIdx := 0; regIdx < 30; regIdx++ {
					key := fmt.Sprintf("x%d", regIdx)
					value, exists := rec[key]
					if !exists {
						continue
					}
					valueText := fmt.Sprintf("0x%X", toInt64(value))
					uid := fmt.Sprintf("%s.f%s", xregsUID, key)
					recordUIDMap[uid] = flat
					fieldUIDMap[uid] = key
					valueUIDMap[uid] = valueText
					addLeaf(uid, xregsUID, fmt.Sprintf("    X%d: %s  [%s]", regIdx, valueText, hwbpRegOp(rec, key)))
				}

				// Q0~Q31
				qregs := hwbpQregs(rec)
				if len(qregs) > 0 {
					qregsUID := recordUID + ".qregs"
					recordUIDMap[qregsUID] = flat
					pointUIDMap[qregsUID] = vp.index
					addBranch(qregsUID, &recordUID)
					texts[qregsUID] = "  SIMD 寄存器快照 Q0~Q31"
					for regIdx, raw := range qregs {
						hi, lo := hwbpQregParts(raw)
						key := fmt.Sprintf("q%d", regIdx)
						valueText := fmt.Sprintf("0x%016X%016X", hi, lo)
						uid := fmt.Sprintf("%s.f%s", qregsUID, key)
						recordUIDMap[uid] = flat
						fieldUIDMap[uid] = key
						valueUIDMap[uid] = valueText
						addLeaf(uid, qregsUID, fmt.Sprintf("    Q%d: 0x%016X_%016X  [%s]", regIdx, hi, lo, hwbpRegOp(rec, key)))
					}
				}

				// write candidates
				if _, writeCount := hwbpMaskCounts(rec); writeCount > 0 {
					writeUID := recordUID + ".write"
					recordUIDMap[writeUID] = flat
					pointUIDMap[writeUID] = vp.index
					addBranch(writeUID, &recordUID)
					texts[writeUID] = "  写入寄存器候选"
					xregs := hwbpXregs(rec)
					x0 := int64(0)
					if len(xregs) > 0 {
						x0 = toInt64(xregs[0])
					}
					x1 := int64(0)
					if len(xregs) > 1 {
						x1 = toInt64(xregs[1])
					}
					valueText := fmt.Sprintf("0x%X", x0)
					uid := writeUID + ".fx0"
					recordUIDMap[uid] = flat
					fieldUIDMap[uid] = "x0"
					valueUIDMap[uid] = valueText
					addLeaf(uid, writeUID, fmt.Sprintf("    候选写入值(X0): %s", valueText))
					addrText := fmt.Sprintf("0x%X", x1)
					uid = writeUID + ".fx1"
					recordUIDMap[uid] = flat
					fieldUIDMap[uid] = "x1"
					valueUIDMap[uid] = addrText
					addLeaf(uid, writeUID, fmt.Sprintf("    候选写入地址(X1): %s", addrText))
				}

				sepUID := recordUID + ".sep"
				recordUIDMap[sepUID] = flat
				pointUIDMap[sepUID] = vp.index
				addLeaf(sepUID, recordUID, "")
			}
		}
	}

	p.recordUIDMap = recordUIDMap
	p.fieldUIDMap = fieldUIDMap
	p.valueUIDMap = valueUIDMap
	p.pointUIDMap = pointUIDMap
	p.treeModel.SetData(roots, children, branch, texts)
	// stale selection cleanup
	if p.selectedIndex >= 0 {
		found := false
		for _, fr := range p.flatRecords {
			if fr.flatIndex == p.selectedIndex {
				found = true
				break
			}
		}
		if !found {
			p.selectedIndex = -1
		}
	}
}

func sortPoints(points []visiblePoint) {
	// selection sort by (hit_addr, index)
	for i := 0; i < len(points); i++ {
		for j := i + 1; j < len(points); j++ {
			hi := toInt64(points[i].point["hit_addr"])
			hj := toInt64(points[j].point["hit_addr"])
			if hj < hi || (hj == hi && points[j].index < points[i].index) {
				points[i], points[j] = points[j], points[i]
			}
		}
	}
}

// ---- hwbp record helpers ----

func hwbpRegOp(rec map[string]any, fieldName string) string {
	opValue := hwbpMaskOpValue(rec, fieldName)
	if opValue != nil {
		if label, ok := hwbpOpLabels[*opValue]; ok {
			return label
		}
		return strconv.Itoa(*opValue)
	}
	return "未设置"
}

func hwbpMaskOpValue(rec map[string]any, fieldName string) *int {
	regIdx := hwbpRegIndexOf(fieldName)
	if regIdx < 0 {
		return nil
	}
	maskRaw, ok := rec["mask"].([]any)
	if !ok {
		return nil
	}
	byteIdx := regIdx >> 2
	if byteIdx >= len(maskRaw) {
		return nil
	}
	byteValue := toInt64(maskRaw[byteIdx]) & 0xFF
	bitOffset := (regIdx & 0x3) << 1
	op := int((byteValue >> uint(bitOffset)) & 0x3)
	return &op
}

func hwbpRegIndexOf(fieldName string) int {
	field := strings.ToLower(strings.TrimSpace(fieldName))
	if idx, ok := hwbpRegIndex[field]; ok {
		return idx
	}
	match := regexp.MustCompile(`^x(\d+)$`).FindStringSubmatch(field)
	if match != nil {
		regIdx, _ := strconv.Atoi(match[1])
		if regIdx >= 0 && regIdx < 30 {
			return hwbpX0RegIndex + regIdx
		}
	}
	match = regexp.MustCompile(`^q(\d+)$`).FindStringSubmatch(field)
	if match != nil {
		regIdx, _ := strconv.Atoi(match[1])
		if regIdx >= 0 && regIdx < 32 {
			return hwbpQ0RegIndex + regIdx
		}
	}
	return -1
}

func hwbpMaskCounts(rec map[string]any) (int, int) {
	readCount, writeCount := 0, 0
	maskRaw, ok := rec["mask"].([]any)
	if !ok {
		return 0, 0
	}
	for regIdx := 0; regIdx < hwbpMaxRegCount; regIdx++ {
		byteIdx := regIdx >> 2
		if byteIdx >= len(maskRaw) {
			break
		}
		byteValue := toInt64(maskRaw[byteIdx]) & 0xFF
		bitOffset := (regIdx & 0x3) << 1
		op := (byteValue >> uint(bitOffset)) & 0x3
		if op == 1 {
			readCount++
		} else if op == 2 {
			writeCount++
		}
	}
	return readCount, writeCount
}

func hwbpOpsSummary(rec map[string]any) string {
	readCount, writeCount := hwbpMaskCounts(rec)
	if writeCount > 0 || readCount > 0 {
		return fmt.Sprintf("读 %d / 写 %d", readCount, writeCount)
	}
	return "未设置"
}

func decodeHwbpRWText(rec map[string]any) string {
	readCount, writeCount := hwbpMaskCounts(rec)
	if writeCount > 0 && readCount > 0 {
		return "读/写"
	}
	if writeCount > 0 {
		return "写入"
	}
	if readCount > 0 {
		return "读取"
	}
	return "未知"
}

func hwbpXregs(rec map[string]any) []any {
	var regs []any
	for regIdx := 0; regIdx < 30; regIdx++ {
		key := fmt.Sprintf("x%d", regIdx)
		if value, exists := rec[key]; exists {
			regs = append(regs, value)
		}
	}
	return regs
}

func hwbpQregs(rec map[string]any) []any {
	var regs []any
	for regIdx := 0; regIdx < 32; regIdx++ {
		key := fmt.Sprintf("q%d", regIdx)
		if value, exists := rec[key]; exists {
			regs = append(regs, value)
		}
	}
	return regs
}

func hwbpQregParts(raw any) (int64, int64) {
	if m, ok := raw.(map[string]any); ok {
		return toInt64(m["hi"]), toInt64(m["lo"])
	}
	value := uint64(toInt64(raw))
	return 0, int64(value)
}

// ---- tree context menu ----

func (p *BpPage) showTreeMenu(pos fyne.Position, uid string) {
	a := p.app
	if uid == "" {
		return
	}
	fieldName := p.fieldUIDMap[uid]
	flatIdx, hasRecord := p.recordUIDMap[uid]
	pointIdx, hasPoint := p.pointUIDMap[uid]

	var menuItems []*fyne.MenuItem
	if fieldName != "" {
		menuItems = append(menuItems, fyne.NewMenuItem("修改寄存器值", func() {
			p.editRecordField(flatIdx, fieldName)
		}))
	}
	if hasRecord {
		menuItems = append(menuItems, fyne.NewMenuItem("复制当前记录完整JSON", func() {
			if rec := p.recordByFlatIndex(flatIdx); rec != nil {
				payload, _ := json.MarshalIndent(rec, "", "  ")
				a.Win.Clipboard().SetContent(string(payload))
				a.setStatus(fmt.Sprintf("已复制 record[%d] JSON", flatIdx))
			}
		}))
	} else if hasPoint {
		menuItems = append(menuItems, fyne.NewMenuItem("复制当前 point 完整JSON", func() {
			payload := p.pointPayload(pointIdx)
			if payload != nil {
				data, _ := json.MarshalIndent(payload, "", "  ")
				a.Win.Clipboard().SetContent(string(data))
				a.setStatus(fmt.Sprintf("已复制 point[%d] JSON", pointIdx))
			}
		}))
	}
	if len(menuItems) == 0 {
		return
	}
	menu := fyne.NewMenu("", menuItems...)
	pop := widget.NewPopUpMenu(menu, a.Win.Canvas())
	pop.ShowAtPosition(pos)
}

func (p *BpPage) recordByFlatIndex(flatIdx int) map[string]any {
	for _, fr := range p.flatRecords {
		if fr.flatIndex == flatIdx {
			return fr.record
		}
	}
	return nil
}

func (p *BpPage) pointPayload(pointIdx int) map[string]any {
	if p.bpInfoData == nil {
		return nil
	}
	bpInfo, _ := p.bpInfoData["bp_info"].(map[string]any)
	if bpInfo == nil {
		return nil
	}
	pointsRaw, _ := bpInfo["points"].([]any)
	if pointIdx < 0 || pointIdx >= len(pointsRaw) {
		return nil
	}
	point, ok := pointsRaw[pointIdx].(map[string]any)
	if !ok {
		return nil
	}
	records := p.pointRecords(point)
	recordCount := p.pointRecordCount(point)
	if recordCount > len(records) {
		recordCount = len(records)
	}
	matched := records[:recordCount]
	totalHits := int64(0)
	for _, rec := range matched {
		totalHits += toInt64(rec["hit_count"])
	}
	return map[string]any{
		"point_index":    pointIdx,
		"point":          point,
		"record_count":   len(matched),
		"total_hit_count": totalHits,
		"records":        matched,
		"hit_addr":       toInt64(point["hit_addr"]),
	}
}

func (p *BpPage) editRecordField(flatIdx int, fieldName string) {
	a := p.app
	// find the uid that maps to (flatIdx, fieldName) for the current value
	currentValue := "0x0"
	for uid, idx := range p.recordUIDMap {
		if idx == flatIdx && p.fieldUIDMap[uid] == fieldName {
			currentValue = p.valueUIDMap[uid]
			break
		}
	}
	a.askText("修改寄存器", fmt.Sprintf("bp_record[%d].%s =", flatIdx, fieldName), currentValue, func(valueText string) {
		value, ok := parseHwbpHexValue(valueText, fieldName)
		if !ok {
			a.warn("输入提示", "写入值格式无效，请输入十六进制值。")
			return
		}
		formatted := formatHwbpEditValue(fieldName, value)
		a.runOp("breakpoint_record.update", map[string]any{
			"index": flatIdx,
			"field": fieldName,
			"value": formatted,
		}, func(resp *bridge.Response) {
			if !a.notifyIfOpFailed(resp, "写入失败", "") {
				return
			}
			p.selectedIndex = flatIdx
			a.setStatus(fmt.Sprintf("已修改 bp_record[%d].%s = %s", flatIdx, fieldName, formatted))
			p.refresh(true)
		})
	})
}

func parseHwbpHexValue(valueText, fieldName string) (int64, bool) {
	text := strings.NewReplacer("_", "", " ", "").Replace(strings.TrimSpace(valueText))
	if text == "" {
		return 0, false
	}
	if strings.HasPrefix(strings.ToLower(text), "0x") {
		text = text[2:]
	}
	if text == "" || !regexp.MustCompile(`^[0-9A-Fa-f]+$`).MatchString(text) {
		return 0, false
	}
	if strings.HasPrefix(strings.ToLower(fieldName), "q") && len(text) > 32 {
		text = text[len(text)-32:]
	}
	value, err := strconv.ParseUint(text, 16, 64)
	if err != nil {
		return 0, false
	}
	return int64(value), true
}

func formatHwbpEditValue(fieldName string, value int64) string {
	lower := strings.ToLower(fieldName)
	if strings.HasPrefix(lower, "q") {
		return fmt.Sprintf("0x%032X", value)
	}
	if lower == "fpsr" || lower == "fpcr" {
		return fmt.Sprintf("0x%X", value&0xFFFFFFFF)
	}
	return fmt.Sprintf("0x%X", value)
}

func minInt(a, b int) int {
	if a < b {
		return a
	}
	return b
}
