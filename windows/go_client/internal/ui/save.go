package ui

import (
	"fmt"
	"regexp"
	"strings"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/widget"

	"luckystar/internal/bridge"
)

type SavedItem struct {
	Addr   string
	Value  string
	Type   string
	Locked bool
	Note   string
}

type SavePage struct {
	app *App

	countLabel *widget.Label
	clearBtn   *widget.Button
	addrInput  *widget.Entry
	typeCombo  *widget.Select
	addBtn     *widget.Button
	view       *TextArea
	content    fyne.CanvasObject

	items []SavedItem
}

func newSavePage(a *App) *SavePage {
	p := &SavePage{app: a}
	p.countLabel = widget.NewLabel("已保存: 0")
	p.clearBtn = widget.NewButton("清空保存", p.onClear)
	p.addrInput = widget.NewEntry()
	p.addrInput.SetPlaceHolder("输入地址，如 0x12345678")
	p.addrInput.OnSubmitted = func(string) { p.onAdd() }
	p.typeCombo = widget.NewSelect(valueTypeOptions, nil)
	p.typeCombo.SetSelected("I32")
	p.addBtn = widget.NewButton("添加地址", p.onAdd)

	p.view = NewTextArea()
	p.view.SetReadOnly(true)
	p.view.SetPlaceHolder("在扫描结果里右键保存后，这里会显示地址和数据。")
	p.view.SetMenu(func(pos fyne.Position) { p.showContextMenu(pos) })

	row := container.NewHBox(
		p.countLabel,
		p.clearBtn,
		widget.NewLabel("手动添加"),
		fixWidth(p.addrInput, 300),
		widget.NewLabel("类型"),
		p.typeCombo,
		p.addBtn,
	)
	p.content = widget.NewCard("保存的地址", "",
		container.NewBorder(container.NewVBox(row), nil, nil, nil, p.view),
	)
	return p
}

func (p *SavePage) Content() fyne.CanvasObject { return p.content }

func (p *SavePage) invalidate() {
	p.items = nil
	p.countLabel.SetText("已保存: 0")
	p.view.Display("")
}

func (p *SavePage) refreshState(silent bool) {
	a := p.app
	apply := func(resp *bridge.Response) {
		if !respOK(resp) {
			if !silent {
				a.warn("保存状态失败", "获取保存列表失败："+respError(resp))
			}
			return
		}
		p.adoptState(respData(resp))
	}
	if silent {
		a.runOpQuiet("saved.list", nil, apply)
	} else {
		a.runOp("saved.list", nil, apply)
	}
}

func (p *SavePage) adoptState(data map[string]any) {
	states, err := bridge.ParseSavedStates(data)
	if err != nil {
		p.app.warn("保存状态失败", "服务端保存地址响应格式异常。")
		return
	}
	p.items = p.items[:0]
	for _, state := range states {
		p.items = append(p.items, SavedItem{
			Addr:   state.AddressHex,
			Value:  state.Value,
			Type:   state.ValueTypeLabel,
			Locked: state.Locked,
			Note:   state.Note,
		})
	}
	p.render()
}

func (p *SavePage) render() {
	p.countLabel.SetText(fmt.Sprintf("已保存: %d", len(p.items)))
	if len(p.items) == 0 {
		p.view.Display("")
		return
	}
	var sb strings.Builder
	for idx, item := range p.items {
		valueText := item.Value
		if valueText == "" {
			valueText = "--"
		}
		lockText := "未锁"
		if item.Locked {
			lockText = "锁定"
		}
		line := fmt.Sprintf("%d. %s | %s | %s | %s", idx+1, item.Addr, valueText, item.Type, lockText)
		if item.Note != "" {
			line += " | 备注: " + item.Note
		}
		sb.WriteString(line + "\n")
	}
	p.view.Display(strings.TrimRight(sb.String(), "\n"))
}

func (p *SavePage) onAdd() {
	a := p.app
	addrText := strings.TrimSpace(p.addrInput.Text)
	addr, err := parseBase0(addrText)
	if err != nil || addr < 0 {
		a.warn("输入提示", "请输入有效的十六进制地址，例如 0x12345678。")
		return
	}
	params, err := bridge.BuildSavedAddParams(addr, valueTypeTokens[p.typeCombo.Selected], "numeric", 64, "")
	if err != nil {
		a.warn("输入提示", err.Error())
		return
	}
	a.runOp("saved.add", params, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "添加失败", "") {
			return
		}
		p.addrInput.SetText("")
		addrText := formatAddrShort(addr)
		a.setStatus("已手动添加地址: " + addrText)
		p.refreshState(true)
	})
}

func (p *SavePage) onClear() {
	a := p.app
	a.runOp("saved.clear", nil, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "清空失败", "") {
			return
		}
		p.items = nil
		p.render()
		a.setStatus("保存页已清空")
	})
}

// ---- context menu ----

var savedIndexPattern = regexp.MustCompile(`^\s*(\d+)\.`)

func (p *SavePage) selectedIndices() []int {
	lines := p.selectedLines()
	var indices []int
	for _, line := range lines {
		if match := savedIndexPattern.FindStringSubmatch(line); match != nil {
			if idx, err := parseBase0(match[1]); err == nil {
				zeroBased := int(idx) - 1
				if zeroBased >= 0 && zeroBased < len(p.items) {
					indices = append(indices, zeroBased)
				}
			}
		}
	}
	return indices
}

func (p *SavePage) selectedLines() []string {
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

func (p *SavePage) showContextMenu(pos fyne.Position) {
	a := p.app
	indices := p.selectedIndices()
	if len(indices) == 0 {
		return
	}
	items := make([]*fyne.MenuItem, 0, 6)
	if len(indices) == 1 {
		item := &p.items[indices[0]]
		items = append(items,
			fyne.NewMenuItem("编辑备注", func() { p.editNote(item) }),
		)
		if item.Note != "" {
			items = append(items, fyne.NewMenuItem("清空备注", func() { p.setNote(item, "") }))
		}
		items = append(items,
			fyne.NewMenuItem("改写值", func() { p.writeValue(item) }),
		)
		lockLabel := "锁定此项"
		if item.Locked {
			lockLabel = "取消锁定"
		}
		items = append(items, fyne.NewMenuItem(lockLabel, func() { p.toggleLock(item) }))
		items = append(items, fyne.NewMenuItem("删除此项", func() { p.removeItems(indices) }))
	} else {
		hasUnlocked := false
		hasLocked := false
		for _, idx := range indices {
			if p.items[idx].Locked {
				hasLocked = true
			} else {
				hasUnlocked = true
			}
		}
		if hasUnlocked {
			items = append(items, fyne.NewMenuItem(fmt.Sprintf("锁定 (%d 项)", len(indices)), func() { p.setLocks(indices, true) }))
		}
		if hasLocked {
			items = append(items, fyne.NewMenuItem(fmt.Sprintf("取消锁定 (%d 项)", len(indices)), func() { p.setLocks(indices, false) }))
		}
		items = append(items, fyne.NewMenuItem(fmt.Sprintf("删除所选项 (%d 项)", len(indices)), func() { p.removeItems(indices) }))
	}
	menu := fyne.NewMenu("", items...)
	pop := widget.NewPopUpMenu(menu, a.Win.Canvas())
	pop.ShowAtPosition(pos)
}

func (p *SavePage) editNote(item *SavedItem) {
	a := p.app
	a.askText("文字备注", fmt.Sprintf("为地址 %s 设置备注：", item.Addr), item.Note, func(text string) {
		normalized := normalizeNote(text)
		if normalized == item.Note {
			return
		}
		p.setNote(item, normalized)
	})
}

func normalizeNote(text string) string {
	lines := strings.Split(text, "\n")
	var parts []string
	for _, line := range lines {
		if trimmed := strings.TrimSpace(line); trimmed != "" {
			parts = append(parts, trimmed)
		}
	}
	return strings.Join(parts, " ")
}

func (p *SavePage) setNote(item *SavedItem, note string) {
	a := p.app
	a.runOp("saved.note.set", map[string]any{"address": item.Addr, "note": note}, func(resp *bridge.Response) {
		if !a.notifyIfOpFailed(resp, "备注失败", "") {
			return
		}
		item.Note = note
		p.render()
		if note == "" {
			a.setStatus("已清空备注: " + item.Addr)
		} else {
			a.setStatus("已更新备注: " + item.Addr)
		}
	})
}

func (p *SavePage) writeValue(item *SavedItem) {
	a := p.app
	a.askText("改写值", fmt.Sprintf("写入地址 %s (%s)：", item.Addr, item.Type), item.Value, func(text string) {
		value := strings.TrimSpace(text)
		if value == "" {
			a.warn("输入提示", "值不能为空。")
			return
		}
		a.runOp("saved.write", map[string]any{"address": item.Addr, "value": value}, func(resp *bridge.Response) {
			if !a.notifyIfOpFailed(resp, "写入失败", "") {
				return
			}
			item.Value = value
			p.render()
			a.setStatus(fmt.Sprintf("已写入: %s = %s", item.Addr, value))
		})
	})
}

func (p *SavePage) toggleLock(item *SavedItem) {
	p.setLocksFor([]int{p.indexOf(item)}, !item.Locked)
}

func (p *SavePage) indexOf(target *SavedItem) int {
	for i := range p.items {
		if &p.items[i] == target {
			return i
		}
	}
	return -1
}

func (p *SavePage) setLocks(indices []int, locked bool) {
	p.setLocksFor(indices, locked)
}

func (p *SavePage) setLocksFor(indices []int, locked bool) {
	a := p.app
	go func() {
		success, failed := 0, 0
		type update struct {
			index int
			ok    bool
		}
		results := make([]update, 0, len(indices))
		for _, idx := range indices {
			value := ""
			if locked {
				value = p.items[idx].Value
			}
			resp, err := a.Br.CallOperation("saved.lock.set", map[string]any{
				"address": p.items[idx].Addr,
				"locked":  locked,
				"value":   value,
			})
			ok := err == nil && resp.Ok
			if ok {
				success++
			} else {
				failed++
			}
			results = append(results, update{index: idx, ok: ok})
		}
		fyne.Do(func() {
			for _, r := range results {
				if r.ok {
					p.items[r.index].Locked = locked
				}
			}
			p.render()
			label := "锁定"
			if !locked {
				label = "取消锁定"
			}
			a.setStatus(fmt.Sprintf("%s：成功 %d 项，失败 %d 项", label, success, failed))
		})
	}()
}

func (p *SavePage) removeItems(indices []int) {
	a := p.app
	go func() {
		success, failed := 0, 0
		var remaining []SavedItem
		removed := map[int]bool{}
		for _, idx := range indices {
			resp, err := a.Br.CallOperation("saved.remove", map[string]any{"address": p.items[idx].Addr})
			if err == nil && resp.Ok {
				success++
				removed[idx] = true
			} else {
				failed++
			}
		}
		for i := range p.items {
			if !removed[i] {
				remaining = append(remaining, p.items[i])
			}
		}
		fyne.Do(func() {
			p.items = remaining
			p.render()
			if failed > 0 {
				a.warn("删除失败", fmt.Sprintf("成功 %d 项，失败 %d 项", success, failed))
			}
			a.setStatus(fmt.Sprintf("已删除 %d 项，失败 %d 项", success, failed))
		})
	}()
}
