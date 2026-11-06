package glance

import "math"

var PixelTextDefaults = Object{"font": "5x7", "align": "center", "valign": "center", "unlit": true}

func contentMatrix(columns, rows int, theme Object) *Matrix {
	return newMatrix(maxInt(1, columns), maxInt(1, rows), themeOptions(theme, Object{"default": Empty}))
}
func fitPixelContent(content *Matrix, region Rect, o Object) *Matrix {
	theme := object(o["theme"])
	gap := GapModeRatios[s(object(theme["layout"]), "gapMode", "")]
	cellFor := func(extent float64, count int) float64 { return extent / (float64(count) + float64(count-1)*gap) }
	cell := math.Min(cellFor(region.Width, content.Columns), cellFor(region.Height, content.Rows))
	if has(o, "maxCellSize") {
		maximum := n(o, "maxCellSize", 0)
		if maximum <= 0 {
			fail("maxCellSize must be positive")
		}
		cell = math.Min(cell, maximum/(1+gap))
	}
	countFor := func(extent float64) int { return toCount(math.Floor((extent/cell+gap)/(1+gap) + 1e-9)) }
	columns, rows := maxInt(content.Columns, countFor(region.Width)), maxInt(content.Rows, countFor(region.Height))
	if columns*rows > MaxMatrixCells {
		fail("pixel content exceeds 4096 cells")
	}
	solved := solveGrid(Object{"region": region, "columns": columns, "rows": rows, "gapMode": object(theme["layout"])["gapMode"]})
	var def any = Empty
	if b(o, "unlit", false) {
		def = theme["default"]
	}
	m := newMatrix(columns, rows, themeOptions(theme, Object{"default": def, "layout": solved["layout"]}))
	offset := func(slack int, position, start, end string) int {
		if position == start {
			return 0
		}
		if position == end {
			return slack
		}
		return int(math.Floor(float64(slack) / 2))
	}
	return m.blit(content, offset(columns-content.Columns, s(o, "align", "center"), "left", "right"), offset(rows-content.Rows, s(o, "valign", "center"), "top", "bottom"))
}
func pixelTextContent(content string, o Object) *Matrix {
	if content == "" {
		fail("pixel text must not be empty")
	}
	theme := resolveTheme(o["theme"])
	font := o["font"]
	if font == nil {
		font = "5x7"
	}
	spacing := Object{}
	for _, k := range []string{"monospace", "letterSpacing", "lineSpacing"} {
		if has(o, k) {
			spacing[k] = o[k]
		}
	}
	metrics := measureText(content, font, spacing)
	w, h := integer(number(metrics["width"])), integer(number(metrics["height"]))
	options := join(spacing, Object{"font": font, "boxWidth": w, "align": s(o, "lineAlign", s(o, "align", "center"))})
	if has(o, "color") {
		options["style"] = o["color"]
	}
	return contentMatrix(w, h, theme).text(content, 0, 0, options)
}
func fitPixelText(content *Matrix, region Rect, o Object) *Matrix {
	options := Object{"theme": resolveTheme(o["theme"]), "align": s(o, "align", "center"), "valign": s(o, "valign", "center"), "unlit": b(o, "unlit", true)}
	if has(o, "maxCellSize") {
		options["maxCellSize"] = o["maxCellSize"]
	}
	return fitPixelContent(content, region, options)
}
func pixelTextPanel(content string, o Object) Object {
	c := canvasOf(o["canvas"])
	_, region := emittedRegion(resolveArea(o["frame"], c), c)
	return fitPixelText(pixelTextContent(content, o), region, o).toPart()
}
func pixelTextRoot(content string, o Object) Object {
	c := canvasOf(o["canvas"])
	return fitPixelText(pixelTextContent(content, o), Rect{0, 0, c.Width, c.Height}, o).toRoot()
}
func PixelTextPanel(content string, o Object) (MatrixPart, error) {
	return safe(func() Object { return pixelTextPanel(content, o) })
}
func PixelTextRoot(content string, o Object) (RootMatrix, error) {
	return safe(func() Object { return pixelTextRoot(content, o) })
}
