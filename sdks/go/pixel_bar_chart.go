package glance

import "math"

var PixelBarChartDefaults = Object{"id": "pixelBarChart", "rows": 10., "gap": .16, "barWidth": 1., "barSpacing": 1., "cornerRadius": .15, "fontSizeRatio": .06, "axisWidth": 1., "zIndex": 0., "colors": Object{"positive": "#5EA2FF", "negative": "#FF6B6B", "track": nil, "axis": "#FFFFFF", "label": "#FFFFFF"}, "axisOpacity": .5, "labelOpacity": .72}

func pixelBarChart(values []float64, o Object) Object {
	if len(values) == 0 {
		fail("pixelBarChart needs at least one value")
	}
	for _, v := range values {
		finite(v)
	}
	c := canvasOf(o["canvas"])
	id, path := identity(o, "pixelBarChart", "pixelBarChart")
	barWidth, spacing, z := integer(n(o, "barWidth", 1)), integer(n(o, "barSpacing", 1)), n(o, "zIndex", 0)
	colors := join(object(PixelBarChartDefaults["colors"]), object(o["colors"]))
	categories := stringsOf(o["categories"])
	showCategories, showValues, showAxes := b(o, "showCategoryLabels", len(categories) > 0), b(o, "showValueLabels", true), b(o, "showAxes", true)
	if showCategories && len(categories) != len(values) {
		fail("categories need one per value")
	}
	lo, hi := 0., 0.
	for _, v := range values {
		lo = math.Min(lo, v)
		hi = math.Max(hi, v)
	}
	lo, hi = n(o, "min", lo), n(o, "max", hi)
	if !has(o, "min") && !has(o, "max") && lo == hi {
		hi = lo + 1
	}
	if !(hi > lo) {
		fail("pixelBarChart max must exceed min")
	}
	zero := clamp(0, lo, hi)
	decimals := decimalsOf(n(o, "decimals", float64(defaultDecimals(lo, hi))))
	ratio := n(o, "fontSizeRatio", .06)
	pt := fontPt(ratio, c)
	lineHeight, labelGap, axisWidth := pt*1.2, pt*.4, n(o, "axisWidth", 1)
	area := resolveArea(o["frame"], c)
	r := area.toPt()
	rows, gap := n(o, "rows", 10), n(o, "gap", .16)
	halfGap := 0.
	if showAxes || showValues || showCategories {
		halfGap = gap * r.Height / rows / 2
	}
	type tick struct {
		value float64
		label string
	}
	ticks := []tick{}
	seen := map[float64]bool{}
	for _, v := range []float64{hi, zero, lo} {
		if !seen[v] {
			ticks = append(ticks, tick{v, formatNumber(v, decimals)})
			seen[v] = true
		}
	}
	labelWidth := 0.
	for _, t := range ticks {
		labelWidth = math.Max(labelWidth, estimateTextWidth(t.label, pt))
	}
	halfLabel, categoryRow := 0., 0.
	if showValues {
		halfLabel = lineHeight * 1.5 / 2
	}
	if showCategories {
		categoryRow = labelGap + textBoxHeight(pt)
	}
	leftInset := 0.
	if showValues {
		leftInset = labelWidth + labelGap
	}
	if showAxes || showValues {
		leftInset += axisWidth + halfGap
	}
	box := area.insetPt(Object{"left": leftInset, "top": halfLabel + halfGap, "bottom": math.Max(halfLabel, categoryRow) + halfGap})
	frame := box.toFrame()
	rect := resolveFrame(frame, c)
	columns := len(values)*barWidth + (len(values)-1)*spacing
	grid := solveGrid(Object{"region": rect, "columns": columns, "rows": rows, "gap": gap})
	geometry := fitMatrix(rect, grid, object(grid["layout"]))
	gridRows := positive(n(grid, "rows", 0))
	baseline := toCount(round(float64(gridRows) * (hi - zero) / (hi - lo)))
	var def any = Empty
	if colors["track"] != nil {
		def = normalizeColor(colors["track"].(string))
	}
	m := newMatrix(columns, gridRows, Object{"default": def, "base": Object{"opacity": 1., "cornerRadius": n(o, "cornerRadius", .15)}, "layout": grid["layout"]})
	for i, value := range values {
		x := i * (barWidth + spacing)
		v := clamp(value, lo, hi)
		if v > zero {
			cells := toCount(round((v - zero) / (hi - zero) * float64(baseline)))
			m.rect(x, baseline-cells, barWidth, cells, colors["positive"], Object{})
		} else if v < zero {
			cells := toCount(round((zero - v) / (zero - lo) * float64(gridRows-baseline)))
			m.rect(x, baseline, barWidth, cells, colors["negative"], Object{})
		}
	}
	plot := place(m.toPart(), Object{"id": id + ":plot", "frame": frame, "path": path, "zIndex": z})
	layers := []any{plot}
	axisX := math.Min(columnBoundary(geometry, 0), n(geometry, "originX", 0)-axisWidth/2)
	if showAxes {
		style := Object{"color": colors["axis"], "strokeWidth": axisWidth, "opacity": .5}
		layers = append(layers, axisLine(Object{"id": id + ":axis-value", "at": axisX / c.Width, "from": rowBoundary(geometry, 0) / c.Height, "to": rowBoundary(geometry, float64(gridRows)) / c.Height, "zIndex": z + 1, "style": style}, true), axisLine(Object{"id": id + ":axis-base", "at": rowBoundary(geometry, float64(baseline)) / c.Height, "from": (axisX - axisWidth/2) / c.Width, "to": columnBoundary(geometry, float64(columns)) / c.Width, "zIndex": z + 1, "style": style}, false))
	}
	add := func(role string, rect Rect, content, horizontal, vertical string) {
		layers = append(layers, label(id+":"+role, boxFromPt(rect, c), content, horizontal, vertical, ratio, colors["label"], .72, z+1))
	}
	if showValues {
		height := lineHeight * 1.5
		right := axisX - axisWidth/2 - labelGap
		for i, t := range ticks {
			boundary := baseline
			if t.value == hi {
				boundary = 0
			} else if t.value == lo {
				boundary = gridRows
			}
			center := rowBoundary(geometry, float64(boundary))
			add("value-"+formatNumber(float64(i), 0), Rect{r.X, center - height/2, math.Max(0, right-r.X), height}, t.label, "right", "center")
		}
	}
	if showCategories {
		pitch := float64(barWidth+spacing) * (n(geometry, "cellWidth", 0) + n(geometry, "gapX", 0))
		for i, category := range categories {
			start := i * (barWidth + spacing)
			center := (columnBoundary(geometry, float64(start)) + columnBoundary(geometry, float64(start+barWidth))) / 2
			width := math.Min(pitch, math.Min(2*(center-r.X), 2*(r.X+r.Width-center)))
			add("category-"+formatNumber(float64(i), 0), Rect{center - width/2, rowBoundary(geometry, float64(gridRows)) + labelGap, width, textBoxHeight(pt)}, category, "center", "top")
		}
	}
	return combineComponents(layers)
}
func PixelBarChart(values []float64, o Object) (Composite, error) {
	return safe(func() Object { return pixelBarChart(values, o) })
}
