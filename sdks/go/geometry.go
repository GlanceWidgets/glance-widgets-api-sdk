package glance

import "math"

func canvasOf(value any) Canvas {
	if c, ok := value.(Canvas); ok {
		if c.Width > 0 && c.Height > 0 {
			return c
		}
		fail("canvas must be positive")
	}
	o := object(value)
	c := Canvas{n(o, "width", 0), n(o, "height", 0)}
	if c.Width <= 0 || c.Height <= 0 {
		fail("canvas size in points is required")
	}
	return c
}
func rectOf(value any) Rect {
	if r, ok := value.(Rect); ok {
		return r
	}
	o := object(value)
	return Rect{n(o, "x", 0), n(o, "y", 0), n(o, "width", 0), n(o, "height", 0)}
}
func rectObject(r Rect) Object {
	return Object{"x": r.X, "y": r.Y, "width": r.Width, "height": r.Height}
}
func resolveFrame(frame Object, c Canvas) Rect {
	axis := func(start, end, size string, extent, def float64) (float64, float64) {
		length := clamp(n(frame, size, def), .02, 1.5) * extent
		if has(frame, start) && has(frame, end) {
			length = math.Max(0, extent-n(frame, start, 0)*extent-n(frame, end, 0)*extent)
		}
		position := 0.
		if has(frame, start) {
			position = n(frame, start, 0) * extent
		} else if has(frame, end) {
			position = extent - n(frame, end, 0)*extent - length
		}
		return position, length
	}
	x, w := axis("left", "right", "width", c.Width, .4)
	y, h := axis("top", "bottom", "height", c.Height, .16)
	return Rect{x, y, w, h}
}
func fitMatrix(r Rect, grid, layout Object) Object {
	c, rows := n(grid, "columns", 0), n(grid, "rows", 0)
	a := clamp(n(layout, "cellAspectRatio", 1), .1, 10)
	short := math.Min(a, 1)
	gap, ok := GapModeRatios[s(layout, "gapMode", "default")]
	if !ok {
		gap = .16
	}
	gx, gy, ins := clamp(n(layout, "columnGapRatio", gap), 0, 10)*short, clamp(n(layout, "rowGapRatio", gap), 0, 10)*short, clamp(n(layout, "edgeInsetRatio", 0), -.45, .45)*short
	ch := math.Min(r.Width/(c*a+(c-1)*gx+2*ins), r.Height/(rows+(rows-1)*gy+2*ins))
	cw, gapX, gapY, inset := ch*a, ch*gx, ch*gy, ch*ins
	return Object{"columns": c, "rows": rows, "cellWidth": cw, "cellHeight": ch, "gapX": gapX, "gapY": gapY, "originX": r.X + (r.Width-(c*cw+(c-1)*gapX+2*inset))/2 + inset, "originY": r.Y + (r.Height-(rows*ch+(rows-1)*gapY+2*inset))/2 + inset}
}
func cellRect(g Object, x, y float64) Rect {
	return Rect{n(g, "originX", 0) + x*(n(g, "cellWidth", 0)+n(g, "gapX", 0)), n(g, "originY", 0) + y*(n(g, "cellHeight", 0)+n(g, "gapY", 0)), n(g, "cellWidth", 0), n(g, "cellHeight", 0)}
}
func columnBoundary(g Object, index float64) float64 {
	return n(g, "originX", 0) + index*(n(g, "cellWidth", 0)+n(g, "gapX", 0)) - n(g, "gapX", 0)/2
}
func rowBoundary(g Object, index float64) float64 {
	return n(g, "originY", 0) + index*(n(g, "cellHeight", 0)+n(g, "gapY", 0)) - n(g, "gapY", 0)/2
}
func solveGrid(o Object) Object {
	r := rectOf(o["region"])
	if r.Width <= 0 || r.Height <= 0 {
		fail("grid region must be positive")
	}
	mode := s(o, "gapMode", "default")
	modeGap, ok := GapModeRatios[mode]
	if !ok {
		fail("unknown gapMode")
	}
	gap, inset := n(o, "gap", modeGap), n(o, "edgeInsetRatio", 0)
	if gap < 0 || gap > 10 || math.Abs(inset) > .45 {
		fail("invalid grid gap/inset")
	}
	count := func(extent, cell float64) float64 { return math.Max(1, round((extent/cell+gap-2*inset)/(1+gap))) }
	square := func(extent, count float64) float64 { return extent / (count + (count-1)*gap + 2*inset) }
	rows, columns := n(o, "rows", 0), n(o, "columns", 0)
	hasRows, hasColumns := has(o, "rows") && o["rows"] != nil, has(o, "columns") && o["columns"] != nil
	if !hasRows && !hasColumns {
		cell := n(o, "cellSize", 0)
		if cell <= 0 {
			fail("give rows, columns or positive cellSize")
		}
		rows = count(r.Height, cell)
		hasRows = true
	}
	if !hasRows {
		rows = count(r.Height, square(r.Width, columns))
	}
	if !hasColumns {
		columns = count(r.Width, square(r.Height, rows))
	}
	positive(rows)
	positive(columns)
	if rows*columns > 4096 {
		fail("grid exceeds 4096 cells")
	}
	ratio := r.Width / r.Height
	ex, ey := (columns-1)*gap+2*inset, (rows-1)*gap+2*inset
	a := (ratio*(rows+ey) - ex) / columns
	if a < 1 {
		den := columns + ex - ratio*ey
		if den <= 0 {
			a = 0
		} else {
			a = ratio * rows / den
			if a <= 0 || a > 1 {
				a = 0
			}
		}
	}
	if b(o, "clampAspect", false) {
		if a == 0 {
			a = .1
		}
		a = clamp(a, .1, 10)
	}
	if !(a >= .1 && a <= 10) {
		fail("grid cannot fill region within aspect limits")
	}
	layout := Object{"gapMode": mode, "edgeInsetRatio": inset, "cellAspectRatio": a}
	if has(o, "gap") {
		layout["columnGapRatio"] = gap
		layout["rowGapRatio"] = gap
	}
	return Object{"columns": columns, "rows": rows, "layout": layout}
}

type Box struct {
	Left, Top, Width, Height float64
	Canvas                   *Canvas
}

func newBox(l, t, w, h float64, c *Canvas) *Box {
	finite(l)
	finite(t)
	finite(w)
	finite(h)
	if w < 0 || h < 0 {
		fail("Box dimensions must not be negative")
	}
	return &Box{l, t, w, h, c}
}
func requireValidCanvas(c Canvas) {
	if !(c.Width > 0) || !(c.Height > 0) {
		fail("Invalid canvas %v×%v: width and height must be > 0", c.Width, c.Height)
	}
}
func (box *Box) canvas() Canvas {
	if box.Canvas == nil {
		fail("Box needs canvas size in points")
	}
	requireValidCanvas(*box.Canvas)
	return *box.Canvas
}
func canvasBox(c *Canvas) *Box { return newBox(0, 0, 1, 1, c) }
func boxFromPt(r Rect, c Canvas) *Box {
	requireValidCanvas(c)
	return newBox(r.X/c.Width, r.Y/c.Height, r.Width/c.Width, r.Height/c.Height, &c)
}
func boxFromFrame(f Object, c Canvas) *Box { return boxFromPt(resolveFrame(f, c), c) }
func (box *Box) sub(l, t, w, h float64) *Box {
	return newBox(box.Left+l*box.Width, box.Top+t*box.Height, w*box.Width, h*box.Height, box.Canvas)
}
func insets(value any) Object {
	if o, ok := value.(map[string]any); ok {
		return o
	}
	v := number(value)
	return Object{"left": v, "top": v, "right": v, "bottom": v}
}
func (box *Box) inset(v any) *Box {
	o := insets(v)
	l, t, r, bottom := n(o, "left", 0), n(o, "top", 0), n(o, "right", 0), n(o, "bottom", 0)
	return box.sub(l, t, 1-l-r, 1-t-bottom)
}
func (box *Box) toPt() Rect {
	c := box.canvas()
	return Rect{box.Left * c.Width, box.Top * c.Height, box.Width * c.Width, box.Height * c.Height}
}
func (box *Box) insetPt(v any) *Box {
	r, o := box.toPt(), insets(v)
	l, t, right, bottom := n(o, "left", 0), n(o, "top", 0), n(o, "right", 0), n(o, "bottom", 0)
	return boxFromPt(Rect{r.X + l, r.Y + t, math.Max(0, r.Width-l-right), math.Max(0, r.Height-t-bottom)}, *box.Canvas)
}
func (box *Box) toFrame() Object {
	axis := func(start, size float64) (float64, float64) {
		if size > 1.5 {
			fail("frame exceeds maximum 1.5")
		}
		if size < .02 {
			return round((start+size/2-.01)*1e6) / 1e6, .02
		}
		return round(start*1e6) / 1e6, round(size*1e6) / 1e6
	}
	l, w := axis(box.Left, box.Width)
	t, h := axis(box.Top, box.Height)
	return Object{"left": l, "top": t, "width": w, "height": h}
}
func (box *Box) split(direction string, countOrWeights any, o Object) []*Box {
	weights := []float64{}
	switch countOrWeights.(type) {
	case int, int64, float64:
		count := integer(number(countOrWeights))
		if count < 0 {
			fail("negative split count")
		}
		for i := 0; i < count; i++ {
			weights = append(weights, 1)
		}
	default:
		for _, v := range array(countOrWeights) {
			weights = append(weights, number(v))
		}
	}
	total := 0.
	for _, w := range weights {
		if !(w >= 0) {
			fail("negative split weight")
		}
		total += w
	}
	if !(total > 0) {
		fail("split needs positive total weights")
	}
	vertical := direction == "rows"
	extent, offset := box.Width, box.Left
	if vertical {
		extent, offset = box.Height, box.Top
	}
	gap := n(o, "gap", 0) * extent
	if has(o, "gapPt") {
		c := box.canvas()
		dim := c.Width
		if vertical {
			dim = c.Height
		}
		gap = n(o, "gapPt", 0) / dim
	}
	available := extent - gap*float64(len(weights)-1)
	if available < 0 {
		fail("gaps exceed box")
	}
	parts := []*Box{}
	for _, weight := range weights {
		size := available * weight / total
		if vertical {
			parts = append(parts, newBox(box.Left, offset, box.Width, size, box.Canvas))
		} else {
			parts = append(parts, newBox(offset, box.Top, size, box.Height, box.Canvas))
		}
		offset += size + gap
	}
	return parts
}
func frameOf(v any) Object {
	if box, ok := v.(*Box); ok {
		return box.toFrame()
	}
	return join(object(v))
}
func ResolveFrame(frame Object, canvas Canvas) (Rect, error) {
	return safe(func() Rect { return resolveFrame(frame, canvasOf(canvas)) })
}
func FitMatrix(region Rect, grid, layout Object) (Object, error) {
	return safe(func() Object { return fitMatrix(region, grid, layout) })
}
func CellRect(g Object, x, y float64) (Rect, error) {
	return safe(func() Rect { return cellRect(g, x, y) })
}
func ColumnBoundary(g Object, index float64) (float64, error) {
	return safe(func() float64 { return columnBoundary(g, index) })
}
func RowBoundary(g Object, index float64) (float64, error) {
	return safe(func() float64 { return rowBoundary(g, index) })
}
func ToRatioRect(r Rect, c Canvas) (Rect, error) {
	return safe(func() Rect {
		c = canvasOf(c)
		return Rect{r.X / c.Width, r.Y / c.Height, r.Width / c.Width, r.Height / c.Height}
	})
}
func SolveGrid(o Object) (Object, error) { return safe(func() Object { return solveGrid(o) }) }
func RequireCanvas(canvas *Canvas, what string) (Canvas, error) {
	return safe(func() Canvas {
		if canvas == nil {
			fail("%s needs canvas size in points", what)
		}
		return canvasOf(*canvas)
	})
}
func NewBox(l, t, w, h float64, canvas ...Canvas) (*Box, error) {
	return safe(func() *Box {
		var c *Canvas
		if len(canvas) > 0 {
			c = &canvas[0]
		}
		return newBox(l, t, w, h, c)
	})
}
func CanvasBox(canvas ...Canvas) *Box {
	if len(canvas) > 0 {
		return canvasBox(&canvas[0])
	}
	return canvasBox(nil)
}
func BoxFromPt(rect Rect, canvas Canvas) (*Box, error) {
	return safe(func() *Box { return boxFromPt(rect, canvasOf(canvas)) })
}
func BoxFromFrame(frame Object, canvas Canvas) (*Box, error) {
	return safe(func() *Box { return boxFromFrame(frame, canvasOf(canvas)) })
}
func (box *Box) Right() float64  { return box.Left + box.Width }
func (box *Box) Bottom() float64 { return box.Top + box.Height }
func (box *Box) Sub(l, t, w, h float64) (*Box, error) {
	return safe(func() *Box { return box.sub(l, t, w, h) })
}
func (box *Box) Inset(value any) (*Box, error) { return safe(func() *Box { return box.inset(value) }) }
func (box *Box) InsetPt(value any) (*Box, error) {
	return safe(func() *Box { return box.insetPt(value) })
}
func (box *Box) ToPt() (Rect, error)      { return safe(func() Rect { return box.toPt() }) }
func (box *Box) ToFrame() (Object, error) { return safe(func() Object { return box.toFrame() }) }
func (box *Box) Split(direction string, count any, options ...Object) ([]*Box, error) {
	return safe(func() []*Box { return box.split(direction, count, opt(options)) })
}
func (box *Box) Rows(count any, options ...Object) ([]*Box, error) {
	return box.Split("rows", count, options...)
}
func (box *Box) Columns(count any, options ...Object) ([]*Box, error) {
	return box.Split("columns", count, options...)
}
