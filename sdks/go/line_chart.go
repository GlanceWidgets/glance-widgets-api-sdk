package glance

import (
	"math"
	"sort"
)

var LineChartDefaults = Object{"id": "lineChart", "interpolation": "monotone", "strokeWidth": 2., "fillOpacity": 0., "areaFillOpacity": .3, "markerRadius": 3., "fontSizeRatio": .06, "axisWidth": 1., "gridCellSize": 8., "zIndex": 0., "seriesColors": []string{"#6BA7FF", "#FF9F0A", "#34C759", "#FF453A", "#BF5AF2", "#5AC8FA"}, "axisColor": "#FFFFFF", "labelColor": "#FFFFFF", "axisOpacity": .5, "labelOpacity": .72}

type chartSeries struct {
	Points [][2]float64
	Color  string
}

func preparePoints(entry any) [][2]float64 {
	spec, ok := entry.(map[string]any)
	if !ok {
		spec = Object{"values": entry}
	}
	if has(spec, "values") == has(spec, "points") {
		fail("series needs exactly one of values or points")
	}
	points := [][2]float64{}
	if has(spec, "points") {
		for _, p := range array(spec["points"]) {
			a := array(p)
			if len(a) < 2 {
				fail("points must be pairs")
			}
			points = append(points, [2]float64{finite(number(a[0])), finite(number(a[1]))})
		}
	} else {
		for i, v := range array(spec["values"]) {
			points = append(points, [2]float64{float64(i), finite(number(v))})
		}
	}
	if len(points) < 2 || len(points) > 1024 {
		fail("series needs2..1024 points")
	}
	sort.Slice(points, func(i, j int) bool { return points[i][0] < points[j][0] })
	for i := 1; i < len(points); i++ {
		if points[i][0] == points[i-1][0] {
			fail("series repeats x")
		}
	}
	return points
}
func seriesMarkers(points [][2]float64, color string, markers Object, decimals int, radius float64, labelColor string) []any {
	type wantedMarker struct {
		index int
		kind  string
	}
	wanted := []wantedMarker{}
	if b(markers, "last", false) {
		wanted = append(wanted, wantedMarker{len(points) - 1, "last"})
	}
	maxIndex, minIndex := 0, 0
	for i, p := range points {
		if p[1] > points[maxIndex][1] {
			maxIndex = i
		}
		if p[1] < points[minIndex][1] {
			minIndex = i
		}
	}
	if b(markers, "max", false) {
		wanted = append(wanted, wantedMarker{maxIndex, "max"})
	}
	if b(markers, "min", false) {
		wanted = append(wanted, wantedMarker{minIndex, "min"})
	}
	seen := map[int]bool{}
	out := []any{}
	for _, want := range wanted {
		if seen[want.index] {
			continue
		}
		seen[want.index] = true
		p := points[want.index]
		at := Object{"x": p[0]}
		marker := Object{"at": at, "symbol": Object{"radiusPt": radius, "fill": color}}
		if b(markers, "labels", false) {
			position := "top"
			offset := -(radius + 2)
			if want.kind == "min" {
				position = "bottom"
				offset = radius + 2
			}
			marker["label"] = formatNumber(p[1], decimals)
			marker["labelStyle"] = Object{"color": labelColor, "fontSizePt": 10., "position": position}
			at["offsetPt"] = []any{0., offset}
		}
		out = append(out, marker)
	}
	return out
}

type plotLayout struct {
	Canvas                                  Canvas
	Area, Plot                              Rect
	CurveFrame                              Object
	Ratio, FontPt, LineHeight, GapPt, AxisX float64
}

func planPlot(c Canvas, s Object, ticks []string, padding float64) *plotLayout {
	ratio := n(s, "fontSizeRatio", .06)
	pt := fontPt(ratio, c)
	axisWidth := n(s, "axisWidth", 1)
	axisPt := 0.
	if b(s, "showAxes", false) {
		axisPt = axisWidth
	}
	labelWidth := 0.
	for _, t := range ticks {
		labelWidth = math.Max(labelWidth, estimateTextWidth(t, pt))
	}
	halfLabel := 0.
	if b(s, "showValueLabels", false) {
		halfLabel = pt * 1.2 * 1.5 / 2
	}
	xRow := 0.
	if len(array(s["xLabels"])) > 0 {
		xRow = pt*.4 + textBoxHeight(pt)
	}
	left := axisPt
	if b(s, "showValueLabels", false) {
		left += labelWidth + pt*.4
	}
	need := Object{"left": left, "top": halfLabel, "right": 0., "bottom": math.Max(halfLabel, axisPt+xRow)}
	insets := Object{}
	for k, v := range need {
		insets[k] = math.Max(number(v), padding) - padding
	}
	area := resolveArea(s["frame"], c)
	frame, rect := emittedRegion(area.insetPt(insets), c)
	plot := Rect{rect.X + padding, rect.Y + padding, rect.Width - 2*padding, rect.Height - 2*padding}
	if plot.Width <= 0 || plot.Height <= 0 {
		fail("chart area too small for labels")
	}
	return &plotLayout{c, area.toPt(), plot, frame, ratio, pt, pt * 1.2, pt * .4, plot.X - axisWidth/2}
}
func chartGrid(id, path string, z float64, grid Object, layout *plotLayout) Object {
	theme := resolveTheme(grid["theme"])
	frame, region := emittedRegion(boxFromPt(layout.Plot, layout.Canvas), layout.Canvas)
	solved := solveGrid(Object{"region": region, "cellSize": n(grid, "cellSize", 8), "gapMode": object(theme["layout"])["gapMode"]})
	m := newMatrix(positive(n(solved, "columns", 0)), positive(n(solved, "rows", 0)), themeOptions(theme, Object{"default": theme["default"], "layout": solved["layout"]}))
	placed := place(m.toPart(), Object{"id": id + ":grid", "frame": frame, "path": path, "zIndex": z})
	layout.Plot = region
	return placed
}
func chartOverlays(id string, z float64, s Object, l *plotLayout, ticks []string) []any {
	c, area, plot := l.Canvas, l.Area, l.Plot
	axisWidth := n(s, "axisWidth", 1)
	bottom := plot.Y + plot.Height
	layers := []any{}
	colors := object(s["colors"])
	if b(s, "showAxes", false) {
		style := Object{"color": svalue(colors, "axis", "#FFFFFF"), "strokeWidth": axisWidth, "opacity": .5}
		layers = append(layers, axisLine(Object{"id": id + ":axis-value", "at": l.AxisX / c.Width, "from": plot.Y / c.Height, "to": bottom / c.Height, "zIndex": z, "style": style}, true), axisLine(Object{"id": id + ":axis-base", "at": (bottom + axisWidth/2) / c.Height, "from": (l.AxisX - axisWidth/2) / c.Width, "to": (plot.X + plot.Width) / c.Width, "zIndex": z, "style": style}, false))
	}
	add := func(role string, rect Rect, content, horizontal, vertical string) {
		layers = append(layers, label(id+":"+role, boxFromPt(rect, c), content, horizontal, vertical, l.Ratio, svalue(colors, "label", "#FFFFFF"), .72, z))
	}
	if b(s, "showValueLabels", false) {
		height := l.LineHeight * 1.5
		right := l.AxisX - axisWidth/2 - l.GapPt
		for i, content := range ticks {
			center := bottom
			if i == 0 {
				center = plot.Y
			}
			add("value-"+formatNumber(float64(i), 0), Rect{area.X, center - height/2, math.Max(0, right-area.X), height}, content, "right", "center")
		}
	}
	xLabels := stringsOf(s["xLabels"])
	axisPt := 0.
	if b(s, "showAxes", false) {
		axisPt = axisWidth
	}
	for i, content := range xLabels {
		share := .5
		if len(xLabels) > 1 {
			share = float64(i) / float64(len(xLabels)-1)
		}
		width := estimateTextWidth(content, l.FontPt)
		left := math.Min(area.X+area.Width-width, math.Max(area.X, plot.X+share*plot.Width-width/2))
		add("x-"+formatNumber(float64(i), 0), Rect{left, bottom + axisPt + l.GapPt, width, textBoxHeight(l.FontPt)}, content, "center", "top")
	}
	return layers
}
func svalue(o Object, key, def string) string { return s(o, key, def) }
func buildChart(what string, input []any, settings Object) Object {
	if len(input) == 0 {
		fail("chart needs at least one series")
	}
	id, path := identity(settings, "lineChart", what)
	z, stroke := n(settings, "zIndex", 0), n(settings, "strokeWidth", 2)
	markers := object(settings["markers"])
	radius := n(markers, "radius", 3)
	hasMarkers := b(markers, "last", false) || b(markers, "min", false) || b(markers, "max", false)
	colors := object(settings["colors"])
	palette := stringsOf(LineChartDefaults["seriesColors"])
	if has(colors, "series") {
		palette = stringsOf(colors["series"])
	}
	if len(palette) == 0 {
		fail("series palette must not be empty")
	}
	series := []chartSeries{}
	for i, entry := range input {
		color := palette[i%len(palette)]
		if spec, ok := entry.(map[string]any); ok {
			color = s(spec, "color", color)
		}
		series = append(series, chartSeries{preparePoints(entry), normalizeColor(color)})
	}
	first := series[0].Points[0]
	xmin, xmax, lo, hi := first[0], first[0], first[1], first[1]
	for _, entry := range series {
		for _, p := range entry.Points {
			xmin = math.Min(xmin, p[0])
			xmax = math.Max(xmax, p[0])
			lo = math.Min(lo, p[1])
			hi = math.Max(hi, p[1])
		}
	}
	lo, hi = n(settings, "min", lo), n(settings, "max", hi)
	if lo == hi && !has(settings, "min") && !has(settings, "max") {
		pad := 1.
		if math.Abs(lo) >= .001 {
			pad = math.Abs(lo) * .1
		}
		lo -= pad
		hi += pad
	}
	if !(hi > lo) {
		fail("chart max must exceed min")
	}
	decimals := decimalsOf(n(settings, "decimals", float64(defaultDecimals(lo, hi))))
	labelColor := s(colors, "label", "#FFFFFF")
	markerLists := [][]any{}
	reach := 0.
	for i, entry := range series {
		for j, p := range entry.Points {
			series[i].Points[j] = [2]float64{p[0], clamp(p[1], lo, hi)}
		}
		list := seriesMarkers(series[i].Points, entry.Color, markers, decimals, radius, labelColor)
		markerLists = append(markerLists, list)
		for _, v := range list {
			m := object(v)
			if has(m, "label") {
				reach = math.Max(reach, math.Max(radius+2+textBoxHeight(10), estimateTextWidth(s(m, "label", ""), 10)/2))
			}
		}
	}
	markerRadius := 0.
	if hasMarkers {
		markerRadius = radius
	}
	padding := math.Min(48, math.Max(stroke/2, math.Max(markerRadius, reach)))
	var grid Object
	if value, ok := settings["grid"]; ok && value != nil && value != false {
		if value == true {
			grid = Object{}
		} else {
			grid = object(value)
		}
	}
	ticks := []string{formatNumber(hi, decimals), formatNumber(lo, decimals)}
	needsCanvas := b(settings, "showAxes", false) || b(settings, "showValueLabels", false) || len(array(settings["xLabels"])) > 0 || grid != nil
	var layout *plotLayout
	var frame Object
	if needsCanvas {
		layout = planPlot(canvasOf(settings["canvas"]), settings, ticks, padding)
		frame = layout.CurveFrame
	} else if settings["frame"] != nil {
		frame = frameOf(settings["frame"])
	} else {
		frame = Object{"left": 0., "top": 0., "width": 1., "height": 1.}
	}
	layers := []any{}
	if layout != nil && grid != nil {
		background := chartGrid(id, path, z, grid, layout)
		layers = append(layers, background)
	}
	for i, entry := range series {
		points := []any{}
		for _, p := range entry.Points {
			points = append(points, []any{p[0], p[1]})
		}
		layers = append(layers, curve(Object{"id": id + ":series-" + formatNumber(float64(i), 0), "frame": frame, "zIndex": z + 1, "data": Object{"mode": "samples", "points": points, "domain": []any{xmin, xmax}, "yDomain": []any{lo, hi}, "interpolation": s(settings, "interpolation", "monotone"), "markers": markerLists[i]}, "style": Object{"color": entry.Color, "fillColor": entry.Color, "fillOpacity": n(settings, "fillOpacity", 0), "opacity": 1., "strokeWidth": stroke, "plotPadding": padding}}))
	}
	if layout != nil {
		layers = append(layers, chartOverlays(id, z+2, settings, layout, ticks)...)
	}
	return combineComponents(layers)
}
func lineChart(series []any, o Object) Object {
	return buildChart("lineChart", series, join(o, Object{"showAxes": b(o, "showAxes", true), "showValueLabels": b(o, "showValueLabels", true)}))
}
func areaChart(series []any, o Object) Object {
	return buildChart("areaChart", series, join(o, Object{"fillOpacity": n(o, "fillOpacity", .3), "showAxes": b(o, "showAxes", true), "showValueLabels": b(o, "showValueLabels", true)}))
}
func sparkline(values []float64, o Object) Object {
	rest := join(o)
	delete(rest, "color")
	delete(rest, "fill")
	delete(rest, "fillOpacity")
	entry := Object{"values": values}
	if has(o, "color") {
		entry["color"] = o["color"]
	}
	markers := o["markers"]
	if markers == nil {
		markers = Object{"last": true}
	}
	fill := 0.
	if b(o, "fill", false) {
		fill = n(o, "fillOpacity", .3)
	}
	return buildChart("sparkline", []any{entry}, join(rest, Object{"id": s(o, "id", "sparkline"), "markers": markers, "fillOpacity": fill, "showAxes": false, "showValueLabels": false}))
}
func LineChart(series []any, options ...Object) (Composite, error) {
	return safe(func() Object { return lineChart(series, opt(options)) })
}
func AreaChart(series []any, options ...Object) (Composite, error) {
	return safe(func() Object { return areaChart(series, opt(options)) })
}
func Sparkline(values []float64, options ...Object) (Composite, error) {
	return safe(func() Object { return sparkline(values, opt(options)) })
}
