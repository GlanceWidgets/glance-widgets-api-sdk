package glance

import (
	"math"
)

// Modern (layer-built, not matrix) components: progressBar, ringProgress, barChart, battery, signal.
// Thin or partial pieces are drawn with round-capped `line` (stroke 0.5-8 pt) and `curve` (0.5-12 pt) layers.

var ModernColors = Object{"fill": "#0A84FF", "track": "#8E8E93", "label": "#FFFFFF"}
var ProgressBarDefaults = Object{"id": "progress", "min": 0., "max": 100., "orientation": "horizontal", "thickness": 6., "labelPosition": "end", "fontSizeRatio": .07, "labelOpacity": 1., "zIndex": 0.}
var RingProgressDefaults = Object{"id": "ring", "min": 0., "max": 100., "startAngle": 0., "thicknessRatio": .1, "thicknessMin": 3., "labelOpacity": 1., "zIndex": 0.}
var BarChartDefaults = Object{"id": "barChart", "fontSizeRatio": .06, "barShareOfSlot": .5, "baselineWidth": 1., "baselineOpacity": .3, "labelOpacity": .72, "zIndex": 0., "colors": Object{"positive": "#0A84FF", "negative": "#FF453A", "baseline": "#FFFFFF", "label": "#FFFFFF"}}
var BatteryDefaults = Object{"id": "battery", "min": 0., "max": 100., "height": 12., "widthRatio": 2.3, "fontSizeRatio": .07, "zIndex": 0.}
var SignalDefaults = Object{"id": "signal", "min": 0., "max": 100., "bars": 4., "maxHeight": 14., "barWidthRatio": .26, "gapRatio": .5, "shortestRatio": .35, "zIndex": 0.}

const (
	TrackOpacity     = .3
	LineStrokeMaxPt  = 8.
	CurveStrokeMaxPt = 12.
	BadLevel         = .2
	WarnLevel        = .4
	labelMaxShare    = 3.
	sampleStepDeg    = 2.
	ringDecimals     = 1e4
	batteryDecimals  = 1e3
	batteryHeightMin = 5.
	batteryInset     = 2.
	batteryTabH      = .5
	batteryTabW      = .25
	batteryTabGap    = 1.
)

func requireStroke(what, name string, value, max float64) {
	if math.IsNaN(value) || math.IsInf(value, 0) || value < .5 || value > max {
		fail("%s: %s must be between 0.5 and %v pt, got %v", what, name, max, value)
	}
}
func colorOf(colors Object, key string, def any) any {
	if v, ok := colors[key]; ok && v != nil {
		return v
	}
	return def
}
func minCapsuleLength(c Canvas, vertical bool, width float64) float64 {
	if vertical {
		return width + FrameSizeMin*c.Height
	}
	return width + FrameSizeMin*c.Width
}
func fillLength(fraction, full, minimum float64) float64 {
	if fraction <= 0 {
		return 0
	}
	return math.Min(full, math.Max(minimum, fraction*full))
}

// capsule is a round-capped line whose visible extent is exactly [from, to].
func capsule(id string, c Canvas, vertical bool, at, from, to, width float64, color any, opacity, z float64) Object {
	start := from + width/2
	var frame *Box
	direction := "horizontal"
	if vertical {
		direction = "vertical"
		end := math.Max(to-width/2, start+FrameSizeMin*c.Height)
		frame = newBox(at/c.Width, start/c.Height, 0, (end-start)/c.Height, nil)
	} else {
		end := math.Max(to-width/2, start+FrameSizeMin*c.Width)
		frame = newBox(start/c.Width, at/c.Height, (end-start)/c.Width, 0, nil)
	}
	return line(Object{"id": id, "frame": frame, "direction": direction, "zIndex": z, "style": Object{"color": color, "opacity": opacity, "strokeWidth": width, "cap": "round"}})
}

// labelWanted: a label option that is absent, null or false means no label (ring: absent means a label).
func labelContent(o Object, fr float64) string {
	if text, ok := o["label"].(string); ok {
		return text
	}
	return PercentLabel(fr)
}
func labelOff(o Object) bool { v, ok := o["label"].(bool); return ok && !v }

func progressBar(value float64, o Object) Object {
	c := canvasOf(o["canvas"])
	id, _ := identity(o, "progress", "progressBar")
	lo, hi := n(o, "min", 0), n(o, "max", 100)
	requireRange(lo, hi)
	thickness := n(o, "thickness", 6)
	requireStroke("progressBar", "thickness", thickness, LineStrokeMaxPt)
	vertical := s(o, "orientation", "horizontal") == "vertical"
	colors := join(ModernColors, object(o["colors"]))
	z := n(o, "zIndex", 0)
	fr := fraction(value, lo, hi)
	a := resolveArea(o["frame"], c).toPt()
	from, to := a.X, a.X+a.Width
	if vertical {
		from, to = a.Y, a.Y+a.Height
	}
	var labelLayers []any
	if set(o, "label") && !labelOff(o) {
		content := labelContent(o, fr)
		atEnd := s(o, "labelPosition", "end") == "end"
		fitAcross, fitAlong := a.Height/labelBoxEm, fontPtForWidth(content, a.Width/labelMaxShare)
		if vertical {
			fitAcross, fitAlong = fontPtForWidth(content, a.Width), a.Height/(labelBoxEm*labelMaxShare)
		}
		requested := fontPt(n(o, "fontSizeRatio", .07), c)
		pt := fontPt(fontRatio(math.Min(requested, math.Min(fitAcross, fitAlong)), c), c)
		slot := estimateTextWidth(content, pt)
		if vertical {
			slot = pt * labelBoxEm
		}
		reserved := slot + pt*.4
		var rect Rect
		horizontal := "left"
		if vertical {
			rect = Rect{a.X, from, a.Width, slot}
			if atEnd {
				rect.Y = to - slot
			}
			horizontal = "center"
		} else {
			rect = Rect{from, a.Y, slot, a.Height}
			if atEnd {
				rect.X = to - slot
				horizontal = "right"
			}
		}
		if atEnd {
			to -= reserved
		} else {
			from += reserved
		}
		labelLayers = append(labelLayers, label(id+":label", boxFromPt(rect, c), content, horizontal, "center", fontRatio(pt, c), colors["label"], 1, z+1))
	}
	at := a.Y + a.Height/2
	if vertical {
		at = a.X + a.Width/2
	}
	minimum := minCapsuleLength(c, vertical, thickness)
	if !(to-from >= minimum) {
		fail("progressBar: the area is too short for a %v pt bar", thickness)
	}
	layers := []any{capsule(id+":track", c, vertical, at, from, to, thickness, colors["track"], TrackOpacity, z)}
	length := fillLength(fr, to-from, minimum)
	if length > 0 {
		start, end := to-length, to
		if vertical == b(o, "reverse", false) {
			start, end = from, from+length
		}
		layers = append(layers, capsule(id+":fill", c, vertical, at, start, end, thickness, colors["fill"], 1, z+1))
	}
	layers = append(layers, labelLayers...)
	return combineComponents(layers)
}

func ringUnit(v float64) float64 { return round(clamp(v, -1, 1)*ringDecimals) / ringDecimals }

// arcPieces: the arc from..to (degrees clockwise from 12 o'clock) as x-ascending polylines on the unit circle.
func arcPieces(from, to float64) [][][2]float64 {
	bounds := []float64{from}
	for angle := math.Ceil((from-90)/180)*180 + 90; angle < to; angle += 180 {
		if angle > from {
			bounds = append(bounds, angle)
		}
	}
	bounds = append(bounds, to)
	pieces := [][][2]float64{}
	for i := 0; i+1 < len(bounds); i++ {
		a, bb := bounds[i], bounds[i+1]
		if !(bb-a > 1e-9) {
			continue
		}
		count := int(math.Max(2, math.Ceil((bb-a)/sampleStepDeg)+1))
		points := make([][2]float64, 0, count)
		for step := 0; step < count; step++ {
			radians := ((a + ((bb-a)*float64(step))/float64(count-1)) * math.Pi) / 180
			points = append(points, [2]float64{ringUnit(math.Sin(radians)), ringUnit(math.Cos(radians))})
		}
		if points[0][0] > points[len(points)-1][0] {
			for i, j := 0, len(points)-1; i < j; i, j = i+1, j-1 {
				points[i], points[j] = points[j], points[i]
			}
		}
		pieces = append(pieces, points)
	}
	return pieces
}

func ringProgress(value float64, o Object) Object {
	c := canvasOf(o["canvas"])
	id, _ := identity(o, "ring", "ringProgress")
	lo, hi := n(o, "min", 0), n(o, "max", 100)
	requireRange(lo, hi)
	start := n(o, "startAngle", 0)
	if math.IsNaN(start) || math.IsInf(start, 0) {
		fail("ringProgress: startAngle must be finite, got %v", start)
	}
	colors := join(ModernColors, object(o["colors"]))
	z := n(o, "zIndex", 0)
	fr := fraction(value, lo, hi)
	a := resolveArea(o["frame"], c).toPt()
	side := math.Min(a.Width, a.Height)
	ring := Rect{a.X + (a.Width-side)/2, a.Y + (a.Height-side)/2, side, side}
	ringBox := boxFromPt(ring, c)
	thickness := math.Min(CurveStrokeMaxPt, math.Max(3, round(side*.1)))
	if set(o, "thickness") {
		thickness = n(o, "thickness", 0)
	}
	requireStroke("ringProgress", "thickness", thickness, CurveStrokeMaxPt)
	if !(thickness < side/2) {
		fail("ringProgress: a %v pt ring does not fit %v pt", thickness, side)
	}
	pieces := func(from, to float64, role string, color any, opacity, zi float64) []any {
		out := []any{}
		for i, pts := range arcPieces(from, to) {
			points := make([]any, len(pts))
			for k, p := range pts {
				points[k] = []any{p[0], p[1]}
			}
			out = append(out, curve(Object{"id": id + ":" + role + "-" + formatNumber(float64(i), 0), "frame": ringBox, "zIndex": zi,
				"data":  Object{"mode": "samples", "points": points, "domain": []any{-1., 1.}, "yDomain": []any{-1., 1.}, "interpolation": "linear"},
				"style": Object{"color": color, "opacity": opacity, "strokeWidth": thickness, "lineCap": "round", "plotPadding": thickness / 2}}))
		}
		return out
	}
	layers := pieces(-90, 360-90, "track", colors["track"], TrackOpacity, z)
	if fr > 0 {
		sweep := fr * 360
		from, to := start, start+sweep
		if b(o, "counterClockwise", false) {
			from, to = start-sweep, start
		}
		layers = append(layers, pieces(from, to, "arc", colors["fill"], 1, z+1)...)
	}
	if !labelOff(o) {
		content := labelContent(o, fr)
		holeRadius := math.Max(1, side/2-thickness)
		holeW, holeH := 1.6*holeRadius, 1.2*holeRadius
		var requested float64
		if set(o, "fontSizeRatio") {
			requested = fontPt(n(o, "fontSizeRatio", 0), c)
		} else {
			requested = math.Min(holeH/labelBoxEm, fontPtForWidth(content, holeW))
		}
		pt := fontPt(fontRatio(requested, c), c)
		bw, bh := math.Max(holeW, estimateTextWidth(content, pt)), math.Max(holeH, textBoxHeight(pt))
		cx, cy := ring.X+side/2, ring.Y+side/2
		layers = append(layers, label(id+":label", boxFromPt(Rect{cx - bw/2, cy - bh/2, bw, bh}, c), content, "center", "center", fontRatio(pt, c), colors["label"], 1, z+2))
	}
	return combineComponents(layers)
}

func barChart(values []float64, o Object) Object {
	if len(values) == 0 {
		fail("barChart: needs at least one value")
	}
	for _, v := range values {
		finite(v)
	}
	c := canvasOf(o["canvas"])
	id, _ := identity(o, "barChart", "barChart")
	colors := join(object(BarChartDefaults["colors"]), object(o["colors"]))
	z := n(o, "zIndex", 0)
	categories := stringsOf(o["categories"])
	if len(categories) > 0 && len(categories) != len(values) {
		fail("barChart: categories must have one entry per value")
	}
	dlo, dhi := 0., 0.
	for _, v := range values {
		dlo, dhi = math.Min(dlo, v), math.Max(dhi, v)
	}
	lo, hi := n(o, "min", dlo), n(o, "max", dhi)
	if !set(o, "min") && !set(o, "max") && lo == hi {
		hi = lo + 1
	}
	if !(hi > lo) {
		fail("barChart: max (%v) must be greater than min (%v)", hi, lo)
	}
	ratio := n(o, "fontSizeRatio", .06)
	pt := fontPt(ratio, c)
	a := resolveArea(o["frame"], c).toPt()
	labelRow := 0.
	if len(categories) > 0 {
		labelRow = pt*.4 + textBoxHeight(pt)
	}
	plotH := a.Height - labelRow
	slot := a.Width / float64(len(values))
	barWidth := math.Min(LineStrokeMaxPt, slot*.5)
	if set(o, "barWidth") {
		barWidth = n(o, "barWidth", 0)
	}
	requireStroke("barChart", "barWidth", barWidth, LineStrokeMaxPt)
	minimum := minCapsuleLength(c, true, barWidth)
	if !(plotH > minimum) {
		fail("barChart: the area is too short")
	}
	yOf := func(v float64) float64 { return a.Y + (1-fraction(v, lo, hi))*plotH }
	zero := math.Min(hi, math.Max(lo, 0))
	baseY := yOf(zero)
	layers := []any{}
	for i, value := range values {
		v := math.Min(hi, math.Max(lo, value))
		if v == zero {
			continue
		}
		valueY := yOf(v)
		from, to, color := math.Min(valueY, baseY-minimum), baseY, colors["positive"]
		if !(v > zero) {
			from, to, color = baseY, math.Max(valueY, baseY+minimum), colors["negative"]
		}
		layers = append(layers, capsule(id+":bar-"+formatNumber(float64(i), 0), c, true, a.X+(float64(i)+.5)*slot, from, to, barWidth, color, 1, z))
	}
	if b(o, "showBaseline", true) {
		layers = append(layers, axisLine(Object{"id": id + ":baseline", "at": baseY / c.Height, "from": a.X / c.Width, "to": (a.X + a.Width) / c.Width, "zIndex": z,
			"style": Object{"color": colors["baseline"], "strokeWidth": 1., "opacity": .3}}, false))
	}
	for i, category := range categories {
		layers = append(layers, label(id+":category-"+formatNumber(float64(i), 0), boxFromPt(Rect{a.X + float64(i)*slot, a.Y + plotH + pt*.4, slot, textBoxHeight(pt)}, c), category, "center", "top", ratio, colors["label"], .72, z))
	}
	return combineComponents(layers)
}

// curveCapsule is a horizontal round-capped bar up to 12 pt thick: a 2-point curve with visible extent [from, to].
func curveCapsule(id string, c Canvas, from, to, at, width float64, color any, opacity, z float64) Object {
	half := width / 2
	r := func(v float64) float64 { return round(v*batteryDecimals) / batteryDecimals }
	fw := math.Max(math.Max(to-from, width+.05), FrameSizeMin*c.Width)
	fh := math.Max(width, FrameSizeMin*c.Height)
	return curve(Object{"id": id, "frame": boxFromPt(Rect{from, at - fh/2, fw, fh}, c), "zIndex": z,
		"data": Object{"mode": "samples",
			"points":        []any{[]any{r(half), .5}, []any{r(fw - half), .5}},
			"domain":        []any{0., r(fw)},
			"yDomain":       []any{0., 1.},
			"interpolation": "linear"},
		"style": Object{"color": color, "opacity": opacity, "strokeWidth": width, "lineCap": "round"}})
}

func battery(level float64, o Object) Object {
	c := canvasOf(o["canvas"])
	id, _ := identity(o, "battery", "battery")
	lo, hi := n(o, "min", 0), n(o, "max", 100)
	requireRange(lo, hi)
	height := n(o, "height", 12)
	if !(height >= batteryHeightMin && height <= CurveStrokeMaxPt) {
		fail("battery: height must be between %v and %v pt, got %v", batteryHeightMin, CurveStrokeMaxPt, height)
	}
	colors := object(o["colors"])
	z := n(o, "zIndex", 0)
	fr := fraction(level, lo, hi)
	track := colorOf(colors, "track", ModernColors["track"])
	status := "good"
	if fr <= BadLevel {
		status = "bad"
	} else if fr <= WarnLevel {
		status = "warn"
	}
	levelColor := colorOf(colors, "fill", colorOf(colors, status, StatusColors[status]))
	a := resolveArea(o["frame"], c).toPt()
	if a.Height < height {
		fail("battery: the area must be at least %v pt high, got %v", height, a.Height)
	}
	tabW := height * batteryTabW
	middle := a.Y + a.Height/2
	reserved := 0.
	var labelLayers []any
	if set(o, "label") && !labelOff(o) {
		content := labelContent(o, fr)
		requested := fontPt(n(o, "fontSizeRatio", .07), c)
		pt := fontPt(fontRatio(math.Min(requested, height/labelBoxEm), c), c)
		slot := estimateTextWidth(content, pt)
		reserved = slot + pt*.4
		labelLayers = []any{label(id+":label", boxFromPt(Rect{a.X + a.Width - slot, a.Y, slot, a.Height}, c), content, "right", "center", fontRatio(pt, c), colorOf(colors, "label", ModernColors["label"]), 1, z+1)}
	}
	available := a.Width - reserved - batteryTabGap - tabW
	bodyW := math.Min(available, n(o, "width", mulF(height, 2.3)))
	if !(bodyW >= height+.1) {
		fail("battery: the area is too narrow (body %v pt)", bodyW)
	}
	left := a.X
	layers := []any{
		curveCapsule(id+":body", c, left, left+bodyW, middle, height, track, TrackOpacity, z),
		capsule(id+":tab", c, true, left+bodyW+batteryTabGap+tabW/2, middle-mulF(height, batteryTabH)/2, middle+mulF(height, batteryTabH)/2, tabW, track, TrackOpacity, z),
	}
	thickness := height - 2*batteryInset
	length := fillLength(fr, bodyW-2*batteryInset, thickness)
	if length > 0 {
		layers = append(layers, curveCapsule(id+":level", c, left+batteryInset, left+batteryInset+length, middle, thickness, levelColor, 1, z+1))
	}
	layers = append(layers, labelLayers...)
	return combineComponents(layers)
}

func signal(level float64, o Object) Object {
	c := canvasOf(o["canvas"])
	id, _ := identity(o, "signal", "signal")
	lo, hi := n(o, "min", 0), n(o, "max", 100)
	requireRange(lo, hi)
	bars := positive(n(o, "bars", 4))
	a := resolveArea(o["frame"], c).toPt()
	height := n(o, "height", math.Min(14, a.Height))
	width := n(o, "barWidth", mulF(height, .26))
	requireStroke("signal", "barWidth", width, LineStrokeMaxPt)
	gap := mulF(width, .5)
	minimum := minCapsuleLength(c, true, width)
	if !(height >= minimum && height <= a.Height) {
		fail("signal: height %v pt does not fit the area", height)
	}
	if mulF(float64(bars), width)+mulF(float64(bars-1), gap) > a.Width {
		fail("signal: the area is too narrow")
	}
	colors := object(o["colors"])
	fill := colorOf(colors, "fill", ModernColors["label"])
	track := colorOf(colors, "track", fill)
	lit := int(round(fraction(level, lo, hi) * float64(bars)))
	bottom := a.Y + (a.Height+height)/2
	layers := make([]any, bars)
	for i := 0; i < bars; i++ {
		color, opacity := track, TrackOpacity
		if i < lit {
			color, opacity = fill, 1.
		}
		share := 1.
		if bars != 1 {
			share = .35 + mulF(1-.35, float64(i))/float64(bars-1)
		}
		layers[i] = capsule(id+":bar-"+formatNumber(float64(i), 0), c, true, a.X+width/2+mulF(float64(i), width+gap), bottom-math.Max(minimum, mulF(height, share)), bottom, width, color, opacity, n(o, "zIndex", 0))
	}
	return combineComponents(layers)
}

func ProgressBar(value float64, o Object) (Composite, error) {
	return safe(func() Object { return progressBar(value, o) })
}
func RingProgress(value float64, o Object) (Composite, error) {
	return safe(func() Object { return ringProgress(value, o) })
}
func BarChart(values []float64, o Object) (Composite, error) {
	return safe(func() Object { return barChart(values, o) })
}
func Battery(level float64, o Object) (Composite, error) {
	return safe(func() Object { return battery(level, o) })
}
func Signal(level float64, o Object) (Composite, error) {
	return safe(func() Object { return signal(level, o) })
}
