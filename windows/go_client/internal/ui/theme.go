package ui

import (
	"image/color"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/theme"
)

// appTheme keeps the default light theme but adjusts a few palette colors
// for a cleaner, lighter look.
type appTheme struct {
	base fyne.Theme
}

func newAppTheme() fyne.Theme {
	return &appTheme{base: theme.LightTheme()}
}

// Theme returns the application theme for the main package to install.
func Theme() fyne.Theme { return newAppTheme() }

func (t *appTheme) Color(name fyne.ThemeColorName, variant fyne.ThemeVariant) color.Color {
	switch name {
	case theme.ColorNameBackground:
		return color.NRGBA{R: 0xF2, G: 0xF4, B: 0xF9, A: 0xFF}
	case theme.ColorNameInputBackground:
		return color.NRGBA{R: 0xFF, G: 0xFF, B: 0xFF, A: 0xFF}
	case theme.ColorNameSeparator:
		return color.NRGBA{R: 0xE2, G: 0xE6, B: 0xEF, A: 0xFF}
	case theme.ColorNamePrimary:
		return color.NRGBA{R: 0x2F, G: 0x6F, B: 0xED, A: 0xFF}
	case theme.ColorNameFocus:
		return color.NRGBA{R: 0x2F, G: 0x6F, B: 0xED, A: 0x80}
	case theme.ColorNameSelection:
		return color.NRGBA{R: 0x2F, G: 0x6F, B: 0xED, A: 0x40}
	case theme.ColorNameHover:
		return color.NRGBA{R: 0x2F, G: 0x6F, B: 0xED, A: 0x12}
	}
	return t.base.Color(name, variant)
}

func (t *appTheme) Font(style fyne.TextStyle) fyne.Resource { return t.base.Font(style) }
func (t *appTheme) Icon(name fyne.ThemeIconName) fyne.Resource {
	return t.base.Icon(name)
}

// Size enlarges the base text size and paddings so CJK glyphs render
// completely inside inputs (Windows + Chinese text gets clipped otherwise).
func (t *appTheme) Size(name fyne.ThemeSizeName) float32 {
	switch name {
	case theme.SizeNameText:
		return 16
	case theme.SizeNameHeadingText:
		return 20
	case theme.SizeNamePadding:
		return 6
	case theme.SizeNameInnerPadding:
		return 12
	case theme.SizeNameLineSpacing:
		return 6
	case theme.SizeNameInlineIcon:
		return 18
	}
	return t.base.Size(name)
}
