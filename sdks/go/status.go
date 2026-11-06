package glance

var StatusStripDefaults = Object{"cellWidth": 1., "cellGap": 1., "rows": 5., "colors": Object{"ok": "#34C759", "warn": "#FF9F0A", "down": "#FF453A"}, "unknownState": "unknown", "names": Object{"ok": "OK", "warn": "Warn", "down": "Down", "unknown": "No data"}}
var LabeledStatusStripDefaults = Object{"id": "statusStrip", "fontSizeRatio": .05, "labelOpacity": .72, "zIndex": 0.}

func stateStyle(state any, theme, colors Object) any {
	name := "unknown"
	if state != nil {
		name = state.(string)
	}
	if color, ok := colors[name]; ok {
		return color
	}
	if name == "unknown" {
		return theme["default"]
	}
	fail("no color for state %q", name)
	return nil
}
func stripMatrix(states []any, o Object) *Matrix {
	if len(states) == 0 {
		fail("status strip needs at least one state")
	}
	width, gap := positive(n(o, "cellWidth", 1)), integer(n(o, "cellGap", 1))
	if gap < 0 {
		fail("cellGap must not be negative")
	}
	if has(o, "rows") {
		positive(n(o, "rows", 0))
	}
	theme := resolveTheme(o["theme"])
	colors := join(object(StatusStripDefaults["colors"]), object(o["colors"]))
	region := regionOf(o, "statusStrip")
	columns := len(states)*width + (len(states)-1)*gap
	var rows any
	if has(o, "rows") {
		rows = o["rows"]
	} else if region == nil {
		rows = 5
	}
	plan := planGrid(region, Object{"columns": columns, "rows": rows}, theme, "statusStrip")
	r := positive(n(plan, "rows", 0))
	m := newMatrix(columns, r, themeOptions(theme, Object{"default": Empty, "layout": plan["layout"]}))
	for i, state := range states {
		m.rect(i*(width+gap), 0, width, r, stateStyle(state, theme, colors), Object{})
	}
	return m
}
func statusStrip(states []any, o Object) Object { return stripMatrix(states, o).toPart() }
func labeledStatusStrip(states []any, o Object) Object {
	c := canvasOf(o["canvas"])
	id, path := identity(o, "statusStrip", "labeledStatusStrip")
	theme := resolveTheme(o["theme"])
	stateColors := join(object(o["colors"]))
	labelColor := stateColors["label"]
	delete(stateColors, "label")
	if labelColor == nil {
		labelColor = object(theme["palette"])["on"]
	}
	colors := join(object(StatusStripDefaults["colors"]), stateColors)
	names := join(object(StatusStripDefaults["names"]), object(o["names"]))
	z, ratio := n(o, "zIndex", 0), n(o, "fontSizeRatio", .05)
	listed := []string{}
	legend := o["legend"]
	if legend != nil && legend != false {
		if _, ok := legend.(bool); !ok {
			listed = stringsOf(legend)
		} else {
			seen := map[string]bool{}
			for _, state := range states {
				name := "unknown"
				if state != nil {
					name = state.(string)
				}
				if !seen[name] {
					listed = append(listed, name)
					seen[name] = true
				}
			}
		}
	}
	pt := fontPt(ratio, c)
	rowPt, gapPt := textBoxHeight(pt), pt*.4
	hasLabels := has(o, "startLabel") || has(o, "endLabel")
	reserved := 0.
	if hasLabels {
		reserved += gapPt + rowPt
	}
	if len(listed) > 0 {
		reserved += gapPt + rowPt
	}
	area := resolveArea(o["frame"], c)
	r := area.toPt()
	if reserved >= r.Height {
		fail("area too small for labels")
	}
	box := area.insetPt(Object{"bottom": reserved})
	frame, _ := emittedRegion(box, c)
	placed := place(stripMatrix(states, join(o, Object{"frame": box, "canvas": c})).toPart(), Object{"id": id + ":strip", "frame": frame, "path": path, "zIndex": z})
	layers := []any{placed}
	rowBox := func(top, left, width float64) *Box { return boxFromPt(Rect{left, top, width, rowPt}, c) }
	add := func(role string, box *Box, content, horizontal string, color any) {
		layers = append(layers, label(id+":"+role, box, content, horizontal, "center", ratio, color, .72, z+1))
	}
	top := r.Y + r.Height - reserved
	if hasLabels {
		top += gapPt
		half := r.Width / 2
		if has(o, "startLabel") {
			add("start", rowBox(top, r.X, half), s(o, "startLabel", ""), "left", labelColor)
		}
		if has(o, "endLabel") {
			add("end", rowBox(top, r.X+half, half), s(o, "endLabel", ""), "right", labelColor)
		}
		top += rowPt
	}
	if len(listed) > 0 {
		top += gapPt
		slot := r.Width / float64(len(listed))
		dotWidth := estimateTextWidth("●", pt)
		nameOffset := dotWidth + pt*.25
		for i, state := range listed {
			left := r.X + float64(i)*slot
			color := stateStyle(state, theme, colors)
			if _, ok := color.(string); !ok {
				color = object(theme["default"])["color"]
			}
			add("legend-dot-"+formatNumber(float64(i), 0), rowBox(top, left, dotWidth), "●", "left", color)
			add("legend-name-"+formatNumber(float64(i), 0), rowBox(top, left+nameOffset, mathMax(0, slot-nameOffset)), s(names, state, state), "left", labelColor)
		}
	}
	return combineComponents(layers)
}
func mathMax(a, b float64) float64 {
	if a > b {
		return a
	}
	return b
}
func StatusStrip(states []any, options ...Object) (MatrixPart, error) {
	return safe(func() Object { return statusStrip(states, opt(options)) })
}
func LabeledStatusStrip(states []any, o Object) (Composite, error) {
	return safe(func() Object { return labeledStatusStrip(states, o) })
}
