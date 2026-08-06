package ui

import (
	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/layout"
	"fyne.io/fyne/v2/widget"
)

// fixWidth wraps an object in a fixed-width cell so inputs across the app
// keep consistent, readable sizes instead of collapsing to their content.
func fixWidth(obj fyne.CanvasObject, width float32) fyne.CanvasObject {
	return container.New(layout.NewGridWrapLayout(fyne.NewSize(width, obj.MinSize().Height)), obj)
}

// TextArea is a monospace multi-line text view that can be made read-only
// while still allowing selection and copy. It supports an optional
// right-click menu and a wheel handler.
type TextArea struct {
	widget.Entry
	readOnly     bool
	programmatic bool
	lastText     string
	onMenu       func(pos fyne.Position)
}

func NewTextArea() *TextArea {
	t := &TextArea{}
	t.ExtendBaseWidget(t)
	t.MultiLine = true
	t.Wrapping = fyne.TextWrapOff
	t.TextStyle = fyne.TextStyle{Monospace: true}
	t.OnChanged = t.handleChanged
	return t
}

func (t *TextArea) handleChanged(text string) {
	if t.programmatic {
		t.lastText = text
		return
	}
	if t.readOnly && text != t.lastText {
		t.programmatic = true
		t.Entry.SetText(t.lastText)
		t.programmatic = false
		return
	}
	t.lastText = text
}

func (t *TextArea) SetReadOnly(ro bool) {
	t.readOnly = ro
	if ro {
		t.lastText = t.Text
	}
}

func (t *TextArea) SetMenu(fn func(pos fyne.Position)) {
	t.onMenu = fn
}

func (t *TextArea) TappedSecondary(pe *fyne.PointEvent) {
	if t.onMenu != nil {
		t.onMenu(pe.AbsolutePosition)
		return
	}
	t.Entry.TappedSecondary(pe)
}

func (t *TextArea) hasFocus() bool {
	app := fyne.CurrentApp()
	if app == nil || app.Driver() == nil {
		return false
	}
	canvas := app.Driver().CanvasForObject(t)
	if canvas == nil {
		return false
	}
	return canvas.Focused() == t
}

// Display sets the text programmatically, preserving the user's selection
// when the widget is focused (mirrors _set_text_preserve_interaction).
func (t *TextArea) Display(text string) {
	if text == t.Text {
		return
	}
	if t.hasFocus() && t.SelectedText() != "" {
		return
	}
	t.programmatic = true
	t.Entry.SetText(text)
	t.programmatic = false
	t.lastText = text
}

// TreeModel is a simple data adapter for widget.Tree.
type TreeModel struct {
	tree     *widget.Tree
	roots    []string
	children map[string][]string
	branch   map[string]bool
	texts    map[string]string
}

func NewTreeModel() *TreeModel {
	m := &TreeModel{}
	m.tree = widget.NewTree(m.childUIDs, m.isBranch, m.createNode, m.updateNode)
	return m
}

func (m *TreeModel) Widget() *widget.Tree { return m.tree }

func (m *TreeModel) childUIDs(uid widget.TreeNodeID) []widget.TreeNodeID {
	if uid == "" {
		return m.roots
	}
	return m.children[uid]
}

func (m *TreeModel) isBranch(uid widget.TreeNodeID) bool { return m.branch[uid] }

func (m *TreeModel) createNode(bool) fyne.CanvasObject { return widget.NewLabel("") }

func (m *TreeModel) updateNode(uid widget.TreeNodeID, _ bool, obj fyne.CanvasObject) {
	obj.(*widget.Label).SetText(m.texts[uid])
}

func (m *TreeModel) SetData(roots []string, children map[string][]string, branch map[string]bool, texts map[string]string) {
	m.roots = roots
	m.children = children
	m.branch = branch
	m.texts = texts
	m.tree.Refresh()
}

func (m *TreeModel) Clear() {
	m.SetData(nil, nil, nil, nil)
}

// MenuTree wraps widget.Tree to add a right-click menu callback and to
// track the currently selected node id (the Tree widget does not expose
// a public Selected() getter).
type MenuTree struct {
	*widget.Tree
	onMenu      func(pos fyne.Position, uid widget.TreeNodeID)
	selectedUID widget.TreeNodeID
}

func NewMenuTree(m *TreeModel) *MenuTree {
	t := &MenuTree{Tree: m.tree}
	m.tree.OnSelected = func(uid widget.TreeNodeID) {
		t.selectedUID = uid
		if t.OnSelected != nil {
			t.OnSelected(uid)
		}
	}
	m.tree.OnUnselected = func(uid widget.TreeNodeID) {
		if t.selectedUID == uid {
			t.selectedUID = ""
		}
		if t.OnUnselected != nil {
			t.OnUnselected(uid)
		}
	}
	return t
}

func (t *MenuTree) TappedSecondary(pe *fyne.PointEvent) {
	if t.onMenu != nil {
		t.onMenu(pe.AbsolutePosition, t.selectedUID)
	}
}

// SetSelectedChanged chains a callback onto the tree's selection changes.
func (t *MenuTree) SetSelectedChanged(fn func(uid widget.TreeNodeID)) {
	t.OnSelected = fn
}
