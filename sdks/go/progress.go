package glance

import "math"

var PixelProgressBarDefaults = Object{"min": 0., "max": 100., "orientation": "horizontal", "segments": 10., "segmentSize": 3., "segmentGap": 1., "thickness": 2.}
var LabeledPixelProgressBarDefaults = Object{"id": "progress", "labelPosition": "end", "fontSizeRatio": .07, "labelOpacity": 1., "zIndex": 0.}
var PixelRingProgressDefaults = Object{"min": 0., "max": 100., "diameter": 33., "thickness": 4., "startAngle": 0.}
var LabeledPixelRingProgressDefaults = Object{"id": "ring", "labelOpacity": 1., "zIndex": 0.}

func barMatrix(value float64, o Object) *Matrix {
	lo, hi := n(o, "min", 0), n(o, "max", 100)
	requireRange(lo, hi)
	segments, size, gap := positive(n(o, "segments", 10)), positive(n(o, "segmentSize", 3)), integer(n(o, "segmentGap", 1))
	if gap < 0 {
		fail("segmentGap must not be negative")
	}
	if has(o, "thickness") {
		positive(n(o, "thickness", 0))
	}
	vertical, reverse := s(o, "orientation", "horizontal") == "vertical", b(o, "reverse", false)
	theme, region := resolveTheme(o["theme"]), regionOf(o, "pixelProgressBar")
	length := segments*size + (segments-1)*gap
	var across any
	if has(o, "thickness") {
		across = o["thickness"]
	} else if region == nil {
		across = 2
	}
	dimensions := Object{"columns": length, "rows": across}
	if vertical {
		dimensions = Object{"rows": length, "columns": across}
	}
	plan := planGrid(region, dimensions, theme, "pixelProgressBar")
	columns, rows := positive(n(plan, "columns", 0)), positive(n(plan, "rows", 0))
	thickness := rows
	if vertical {
		thickness = columns
	}
	on, off := pixelLooks(theme, object(o["colors"]))
	m := newMatrix(columns, rows, themeOptions(theme, Object{"default": Empty, "layout": plan["layout"]}))
	lit := int(round(fraction(value, lo, hi) * float64(segments)))
	for i := 0; i < segments; i++ {
		slot := i
		if vertical != reverse {
			slot = segments - 1 - i
		}
		offset := slot * (size + gap)
		style := off
		if i < lit {
			style = on
		}
		if vertical {
			m.rect(0, offset, thickness, size, style, Object{})
		} else {
			m.rect(offset, 0, size, thickness, style, Object{})
		}
	}
	return m
}
func pixelProgressBar(value float64, o Object) Object { return barMatrix(value, o).toPart() }
func labeledPixelProgressBar(value float64, o Object) Object {
	c := canvasOf(o["canvas"])
	id, path := identity(o, "progress", "labeledPixelProgressBar")
	lo, hi := n(o, "min", 0), n(o, "max", 100)
	requireRange(lo, hi)
	theme := resolveTheme(o["theme"])
	vertical, end := s(o, "orientation", "horizontal") == "vertical", s(o, "labelPosition", "end") == "end"
	content, z := s(o, "label", PercentLabel(fraction(value, lo, hi))), n(o, "zIndex", 0)
	area := resolveArea(o["frame"], c)
	r := area.toPt()
	lineBox := labelBoxEm
	across, along := r.Height/lineBox, fontPtForWidth(content, r.Width/3)
	if vertical {
		across = fontPtForWidth(content, r.Width)
		along = r.Height / (lineBox * 3)
	}
	pt := fontPt(fontRatio(math.Min(fontPt(n(o, "fontSizeRatio", .07), c), math.Min(across, along)), c), c)
	slot := estimateTextWidth(content, pt)
	if vertical {
		slot = pt * lineBox
	}
	reserved := slot + pt*.4
	side := "left"
	if vertical {
		side = "top"
	}
	if end {
		side = "right"
		if vertical {
			side = "bottom"
		}
	}
	barBox := area.insetPt(Object{side: reserved})
	labelRect := Rect{r.X, r.Y, slot, r.Height}
	if end {
		labelRect.X = r.X + r.Width - slot
	}
	if vertical {
		labelRect = Rect{r.X, r.Y, r.Width, slot}
		if end {
			labelRect.Y = r.Y + r.Height - slot
		}
	}
	frame, _ := emittedRegion(barBox, c)
	placed := place(barMatrix(value, join(o, Object{"frame": barBox, "canvas": c})).toPart(), Object{"id": id + ":bar", "frame": frame, "path": path, "zIndex": z})
	horizontal := "left"
	if end {
		horizontal = "right"
	}
	if vertical {
		horizontal = "center"
	}
	color := object(theme["palette"])["on"]
	if v, ok := object(o["colors"])["label"]; ok && v != nil {
		color = v
	}
	layers := []any{placed, label(id+":label", boxFromPt(labelRect, c), content, horizontal, "center", fontRatio(pt, c), color, 1, z+1)}
	return combineComponents(layers)
}
func ringMatrix(value float64, o Object) *Matrix {
	lo, hi := n(o, "min", 0), n(o, "max", 100)
	requireRange(lo, hi)
	diameter, thickness := positive(n(o, "diameter", 33)), positive(n(o, "thickness", 4))
	if diameter < 3 || diameter%2 == 0 {
		fail("ring diameter must be odd and at least3")
	}
	start := n(o, "startAngle", 0)
	theme := resolveTheme(o["theme"])
	on, off := pixelLooks(theme, object(o["colors"]))
	r := (diameter - 1) / 2
	m := newMatrix(diameter, diameter, themeOptions(theme, Object{"default": Empty}))
	sweep := fraction(value, lo, hi) * 360
	m.arc(r, r, r, 0, 360, off, Object{"thickness": thickness})
	if sweep > 0 {
		a, endAngle := start, start+sweep
		if bval := b(o, "counterClockwise", false); bval {
			a, endAngle = start-sweep, start
		}
		m.arc(r, r, r, a, endAngle, on, Object{"thickness": thickness})
	}
	return m
}
func pixelRingProgress(value float64, o Object) Object { return ringMatrix(value, o).toPart() }
func labeledPixelRingProgress(value float64, o Object) Object {
	c := canvasOf(o["canvas"])
	id, path := identity(o, "ring", "labeledPixelRingProgress")
	theme, z := resolveTheme(o["theme"]), n(o, "zIndex", 0)
	content := s(o, "label", PercentLabel(fraction(value, n(o, "min", 0), n(o, "max", 100))))
	area := resolveArea(o["frame"], c).toPt()
	side := math.Min(area.Width, area.Height)
	ringBox := boxFromPt(Rect{area.X + (area.Width-side)/2, area.Y + (area.Height-side)/2, side, side}, c)
	frame, rect := emittedRegion(ringBox, c)
	part := ringMatrix(value, o).toPart()
	placed := place(part, Object{"id": id + ":ring", "frame": frame, "path": path, "zIndex": z})
	d := float64(len(array(array(object(part["content"])["pixels"])[0])))
	g := fitMatrix(rect, Object{"columns": d, "rows": d}, object(object(part["content"])["layout"]))
	center := cellRect(g, (d-1)/2, (d-1)/2)
	hole := math.Max(1, (d-1)/2-n(o, "thickness", 4))
	radius := hole * (n(g, "cellWidth", 0) + n(g, "gapX", 0))
	w, h := 1.6*radius, 1.2*radius
	requested := math.Min(h/labelBoxEm, fontPtForWidth(content, w))
	if has(o, "fontSizeRatio") {
		requested = fontPt(n(o, "fontSizeRatio", 0), c)
	}
	pt := fontPt(fontRatio(requested, c), c)
	bw, bh := math.Max(w, estimateTextWidth(content, pt)), math.Max(h, textBoxHeight(pt))
	color := object(theme["palette"])["on"]
	if v, ok := object(o["colors"])["label"]; ok && v != nil {
		color = v
	}
	layers := []any{placed, label(id+":label", boxFromPt(Rect{center.X + center.Width/2 - bw/2, center.Y + center.Height/2 - bh/2, bw, bh}, c), content, "center", "center", fontRatio(pt, c), color, 1, z+1)}
	return combineComponents(layers)
}
func PixelProgressBar(value float64, options ...Object) (MatrixPart, error) {
	return safe(func() Object { return pixelProgressBar(value, opt(options)) })
}
func LabeledPixelProgressBar(value float64, o Object) (Composite, error) {
	return safe(func() Object { return labeledPixelProgressBar(value, o) })
}
func PixelRingProgress(value float64, options ...Object) (MatrixPart, error) {
	return safe(func() Object { return pixelRingProgress(value, opt(options)) })
}
func LabeledPixelRingProgress(value float64, o Object) (Composite, error) {
	return safe(func() Object { return labeledPixelRingProgress(value, o) })
}
