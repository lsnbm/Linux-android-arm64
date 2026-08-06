package ui

import (
	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/dialog"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
)

func dialogShowWarning(title, message string, parent fyne.Window) {
	content := container.NewHBox(
		widget.NewIcon(theme.WarningIcon()),
		widget.NewLabel(message),
	)
	d := dialog.NewCustom(title, "确定", content, parent)
	d.Show()
}

func dialogShowInformation(title, message string, parent fyne.Window) {
	dialog.ShowInformation(title, message, parent)
}

func dialogShowConfirm(title, message string, onConfirm func(), parent fyne.Window) {
	dialog.ShowConfirm(title, message, func(ok bool) {
		if ok && onConfirm != nil {
			onConfirm()
		}
	}, parent)
}

func dialogAskText(title, message, initial string, onConfirm func(string), parent fyne.Window) {
	d := dialog.NewEntryDialog(title, message, onConfirm, parent)
	d.SetText(initial)
	d.Show()
}
