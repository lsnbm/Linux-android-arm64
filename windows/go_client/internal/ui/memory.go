package ui

import (
	"fmt"
	"strings"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/widget"

	"luckystar/internal/bridge"
)

type MemoryPage struct {
	app *App

	refreshBtn  *widget.Button
	filterInput *widget.Entry
	filterBtn   *widget.Button
	clearBtn    *widget.Button
	dumpInput   *widget.Entry
	dumpBtn     *widget.Button
	view        *TextArea
	content     fyne.CanvasObject

	data map[string]any
}

func newMemoryPage(a *App) *MemoryPage {
	p := &MemoryPage{app: a}
	p.view = NewTextArea()
	p.view.SetReadOnly(true)
	p.view.SetPlaceHolder("点击“刷新内存信息”后显示可读的 memory_info 结构数据。")

	p.filterInput = widget.NewEntry()
	p.filterInput.SetPlaceHolder("输入模块名/地址/权限关键字")
	p.filterInput.OnSubmitted = func(string) { p.onFilter() }
	p.refreshBtn = widget.NewButton("刷新内存信息", p.onRefresh)
	p.filterBtn = widget.NewButton("筛选", p.onFilter)
	p.clearBtn = widget.NewButton("清空筛选", p.onClearFilter)

	p.dumpInput = widget.NewEntry()
	p.dumpInput.SetPlaceHolder("例如 unity 或 0x5000-0x6000")
	p.dumpInput.OnSubmitted = func(string) { p.onDump() }
	p.dumpBtn = widget.NewButton("Dump", p.onDump)

	filterRow := container.NewHBox(
		p.refreshBtn,
		widget.NewLabel("搜索"),
		fixWidth(p.filterInput, 360),
		p.filterBtn,
		p.clearBtn,
	)
	dumpRow := container.NewHBox(
		widget.NewLabel("模块名/地址范围"),
		fixWidth(p.dumpInput, 360),
		p.dumpBtn,
	)
	content := widget.NewCard("内存信息", "",
		container.NewBorder(
			container.NewVBox(filterRow, dumpRow),
			nil, nil, nil,
			p.view,
		),
	)
	p.content = content
	return p
}

func (p *MemoryPage) Content() fyne.CanvasObject { return p.content }

func (p *MemoryPage) invalidate() {
	p.data = nil
	p.view.Display("")
}

func (p *MemoryPage) onRefresh() {
	a := p.app
	a.runOp("memory.map", nil, func(resp *bridge.Response) {
		if !respOK(resp) {
			errText := respError(resp)
			p.view.Display("刷新失败：\n" + errText)
			a.warn("刷新失败", "刷新失败：\n"+errText)
			a.setStatus("内存信息刷新失败")
			return
		}
		data, ok := resp.Data.(map[string]any)
		if !ok {
			p.view.Display("JSON 解析失败：返回数据不是对象")
			a.warn("刷新失败", "JSON 解析失败：返回数据不是对象")
			return
		}
		p.data = data
		p.render()
		a.setStatus(fmt.Sprintf("内存信息刷新成功：模块=%d，区域=%d",
			toInt64(data["module_count"]), toInt64(data["region_count"])))
	})
}

func (p *MemoryPage) onDump() {
	a := p.app
	target := strings.TrimSpace(p.dumpInput.Text)
	if target == "" {
		a.warn("输入提示", "请输入模块名或地址范围。")
		return
	}
	p.dumpBtn.Disable()
	a.setStatus("正在 Dump：" + target)
	a.runOp("memory.dump", map[string]any{"target": target}, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "Dump 失败", "") {
			a.setStatus("Dump 失败")
		} else {
			data := respData(resp)
			path := toString(data["path"])
			if path == "" {
				path = target
			}
			a.info("Dump 完成", "输出路径："+path)
			a.setStatus("Dump 完成：" + path)
		}
		if a.Br.IsConnected() {
			p.dumpBtn.Enable()
		}
	})
}

func (p *MemoryPage) onFilter() {
	if p.data == nil {
		a := p.app
		a.warn("输入提示", "暂无内存信息，请先点击“刷新内存信息”。")
		return
	}
	p.render()
}

func (p *MemoryPage) onClearFilter() {
	p.filterInput.SetText("")
	if p.data != nil {
		p.render()
	}
}

func (p *MemoryPage) render() {
	if p.data == nil {
		p.view.Display("暂无内存信息，请先点击“刷新内存信息”。")
		return
	}
	keyword := strings.ToLower(strings.TrimSpace(p.filterInput.Text))
	info := filterMemoryInfo(p.data, keyword)
	p.view.Display(formatMemoryInfoText(info))
}

func moduleMatchesKeyword(module map[string]any, keyword string) bool {
	if keyword == "" {
		return true
	}
	name := strings.ToLower(toString(module["name"]))
	if strings.Contains(name, keyword) {
		return true
	}
	segs, _ := module["segs"].([]any)
	for _, raw := range segs {
		seg, ok := raw.(map[string]any)
		if !ok {
			continue
		}
		index := toInt64(seg["index"])
		prot := toInt64(seg["prot"])
		start := toInt64(seg["start"])
		end := toInt64(seg["end"])
		if strings.Contains(fmt.Sprintf("%d", index), keyword) ||
			strings.Contains(fmt.Sprintf("%d", prot), keyword) ||
			strings.Contains(strings.ToLower(formatProt(prot)), keyword) ||
			strings.Contains(fmt.Sprintf("0x%x", start), keyword) ||
			strings.Contains(fmt.Sprintf("0x%x", end), keyword) ||
			strings.Contains(fmt.Sprintf("%d", start), keyword) ||
			strings.Contains(fmt.Sprintf("%d", end), keyword) {
			return true
		}
	}
	return false
}

func regionMatchesKeyword(region map[string]any, keyword string) bool {
	if keyword == "" {
		return true
	}
	start := toInt64(region["start"])
	end := toInt64(region["end"])
	return strings.Contains(fmt.Sprintf("0x%x", start), keyword) ||
		strings.Contains(fmt.Sprintf("0x%x", end), keyword) ||
		strings.Contains(fmt.Sprintf("%d", start), keyword) ||
		strings.Contains(fmt.Sprintf("%d", end), keyword)
}

func filterMemoryInfo(info map[string]any, keyword string) map[string]any {
	sourceModuleCount := toInt64(info["module_count"])
	sourceRegionCount := toInt64(info["region_count"])
	modules, _ := info["modules"].([]any)
	regions, _ := info["regions"].([]any)

	filtered := map[string]any{
		"status":            info["status"],
		"module_count":      info["module_count"],
		"region_count":      info["region_count"],
		"_source_module_count": sourceModuleCount,
		"_source_region_count": sourceRegionCount,
		"modules":           modules,
		"regions":           regions,
	}
	if keyword != "" {
		filtered["_filter_keyword"] = keyword
		var keptModules []any
		for _, raw := range modules {
			module, ok := raw.(map[string]any)
			if ok && moduleMatchesKeyword(module, keyword) {
				keptModules = append(keptModules, module)
			}
		}
		var keptRegions []any
		for _, raw := range regions {
			region, ok := raw.(map[string]any)
			if ok && regionMatchesKeyword(region, keyword) {
				keptRegions = append(keptRegions, region)
			}
		}
		filtered["modules"] = keptModules
		filtered["regions"] = keptRegions
	}
	return filtered
}

func formatMemoryInfoText(info map[string]any) string {
	status := toInt64(info["status"])
	moduleCount := toInt64(info["module_count"])
	regionCount := toInt64(info["region_count"])
	sourceModuleCount := toInt64(info["_source_module_count"])
	if sourceModuleCount == 0 {
		sourceModuleCount = moduleCount
	}
	sourceRegionCount := toInt64(info["_source_region_count"])
	if sourceRegionCount == 0 {
		sourceRegionCount = regionCount
	}
	filterKeyword := strings.TrimSpace(toString(info["_filter_keyword"]))

	modules, _ := info["modules"].([]any)
	regions, _ := info["regions"].([]any)

	var sb strings.Builder
	sb.WriteString("MEMORY INFO\n===========\n")
	if filterKeyword != "" {
		fmt.Fprintf(&sb, "STATUS   %d\n", status)
		fmt.Fprintf(&sb, "MODULES  %d / %d\n", len(modules), sourceModuleCount)
		fmt.Fprintf(&sb, "REGIONS  %d / %d\n", len(regions), sourceRegionCount)
		fmt.Fprintf(&sb, "FILTER   %s\n", filterKeyword)
	} else {
		fmt.Fprintf(&sb, "STATUS   %d\n", status)
		fmt.Fprintf(&sb, "MODULES  %d\n", moduleCount)
		fmt.Fprintf(&sb, "REGIONS  %d\n", regionCount)
	}

	sb.WriteString("\nMODULES\n-------\n")
	if len(modules) == 0 {
		sb.WriteString("(none)\n")
	} else {
		for idx, raw := range modules {
			module, ok := raw.(map[string]any)
			if !ok {
				continue
			}
			name := toString(module["name"])
			if name == "" {
				name = "(unnamed)"
			}
			segs, _ := module["segs"].([]any)
			segCount := toInt64(module["seg_count"])
			if segCount == 0 {
				segCount = int64(len(segs))
			}
			fmt.Fprintf(&sb, "[%03d] %s  segments=%d\n", idx+1, name, segCount)
			if len(segs) == 0 {
				sb.WriteString("      (no segments)\n")
				continue
			}
			sb.WriteString("      SEG  PERM    START               END                 SIZE\n")
			sb.WriteString("      ---  ------- ------------------- ------------------- ----------\n")
			for segIdx, rawSeg := range segs {
				seg, ok := rawSeg.(map[string]any)
				if !ok {
					fmt.Fprintf(&sb, "      %3d  (invalid)\n", segIdx+1)
					continue
				}
				segIndex := toInt64(seg["index"])
				protText := formatProt(seg["prot"])
				start := toInt64(seg["start"])
				end := toInt64(seg["end"])
				fmt.Fprintf(&sb, "      %3d  %-7s %s  %s  %10s\n",
					segIndex, protText, formatAddr(start), formatAddr(end), formatSize(end-start))
			}
		}
	}

	sb.WriteString("\nSCAN REGIONS\n------------\n")
	if len(regions) == 0 {
		sb.WriteString("(none)\n")
	} else {
		sb.WriteString("  #   START               END                 SIZE\n")
		sb.WriteString("----  ------------------- ------------------- ----------\n")
		for idx, raw := range regions {
			region, ok := raw.(map[string]any)
			if !ok {
				fmt.Fprintf(&sb, "%4d  (invalid)\n", idx+1)
				continue
			}
			start := toInt64(region["start"])
			end := toInt64(region["end"])
			fmt.Fprintf(&sb, "%4d  %s  %s  %10s\n",
				idx+1, formatAddr(start), formatAddr(end), formatSize(end-start))
		}
	}
	return strings.TrimRight(sb.String(), "\n")
}
