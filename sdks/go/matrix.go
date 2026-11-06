package glance

import (
	"math"
	"reflect"
)

var DefaultPixelBase = Object{"color": "#FFFFFF", "opacity": 1., "cornerRadius": 0., "rotation": 0., "halo": 0.}

type Matrix struct {
	Columns, Rows int
	Layout        Object
	DefaultStyle  any
	Base          Object
	cells         []any
}

func resolvePixel(value any, base Object) any {
	if value == nil {
		return nil
	}
	if value == Empty {
		return Empty
	}
	var merged Object
	if color, ok := value.(string); ok {
		merged = join(base, Object{"color": color})
	} else {
		merged = join(base, object(value))
	}
	return Object{"color": normalizeColor(s(merged, "color", "")), "opacity": clamp(finite(number(merged["opacity"])), 0, 1), "cornerRadius": clamp(finite(number(merged["cornerRadius"])), 0, .5), "rotation": finite(number(merged["rotation"])), "halo": clamp(finite(number(merged["halo"])), 0, 1)}
}
func newMatrix(columns, rows int, o Object) *Matrix {
	if columns < 1 || rows < 1 || columns*rows > 4096 {
		fail("matrix needs positive dimensions, at most 4096 cells")
	}
	requested := object(o["layout"])
	mode := "default"
	if value, exists := requested["gapMode"]; exists {
		var valid bool
		mode, valid = value.(string)
		if !valid {
			fail("gapMode must be a string")
		}
	}
	if _, valid := GapModeRatios[mode]; !valid {
		fail("unknown gapMode")
	}
	layout := Object{
		"gapMode":         mode,
		"edgeInsetRatio":  n(requested, "edgeInsetRatio", 0),
		"cellAspectRatio": n(requested, "cellAspectRatio", 1),
	}
	for _, field := range []string{"columnGapRatio", "rowGapRatio"} {
		if value, exists := requested[field]; exists {
			layout[field] = value
		}
	}
	for field, value := range layout {
		if field != "gapMode" {
			finite(number(value))
		}
	}
	def := o["default"]
	if def == nil {
		def = Empty
	}
	def = resolvePixel(def, DefaultPixelBase)
	visible := DefaultPixelBase
	if def != Empty {
		visible = object(def)
	}
	base := object(resolvePixel(join(visible, object(o["base"])), DefaultPixelBase))
	return &Matrix{columns, rows, layout, def, base, make([]any, columns*rows)}
}
func (m *Matrix) contains(x, y int) bool { return x >= 0 && y >= 0 && x < m.Columns && y < m.Rows }
func (m *Matrix) index(x, y int) int {
	if !m.contains(x, y) {
		fail("cell (%d,%d) outside matrix", x, y)
	}
	return y*m.Columns + x
}
func (m *Matrix) plot(x, y int, cell any) {
	if m.contains(x, y) {
		m.cells[y*m.Columns+x] = cell
	}
}
func (m *Matrix) get(x, y int) any {
	v := m.cells[m.index(x, y)]
	if v == nil {
		return m.DefaultStyle
	}
	return v
}
func (m *Matrix) set(x, y int, style any) *Matrix {
	m.cells[m.index(x, y)] = resolvePixel(style, m.Base)
	return m
}
func (m *Matrix) fill(style any) *Matrix {
	cell := resolvePixel(style, m.Base)
	for i := range m.cells {
		m.cells[i] = cell
	}
	return m
}
func (m *Matrix) blit(other *Matrix, x, y int) *Matrix {
	for r := 0; r < other.Rows; r++ {
		for c := 0; c < other.Columns; c++ {
			cell := other.cells[r*other.Columns+c]
			if cell != nil && cell != Empty {
				m.plot(x+c, y+r, clone(cell))
			}
		}
	}
	return m
}
func rasterLine(x0, y0, x1, y1 int, emit func(int, int)) {
	dx, dy := int(math.Abs(float64(x1-x0))), -int(math.Abs(float64(y1-y0)))
	sx, sy := -1, -1
	if x0 < x1 {
		sx = 1
	}
	if y0 < y1 {
		sy = 1
	}
	err, x, y := dx+dy, x0, y0
	for {
		emit(x, y)
		if x == x1 && y == y1 {
			return
		}
		twice := 2 * err
		if twice >= dy {
			err += dy
			x += sx
		}
		if twice <= dx {
			err += dx
			y += sy
		}
	}
}
func (m *Matrix) line(x0, y0, x1, y1 int, style any) *Matrix {
	cell := resolvePixel(style, m.Base)
	rasterLine(x0, y0, x1, y1, func(x, y int) { m.plot(x, y, cell) })
	return m
}
func (m *Matrix) rect(x, y, w, h int, style any, o Object) *Matrix {
	cell := resolvePixel(style, m.Base)
	fill := b(o, "fill", true)
	for r := y; r < y+h; r++ {
		for c := x; c < x+w; c++ {
			if fill || r == y || r == y+h-1 || c == x || c == x+w-1 {
				m.plot(c, r, cell)
			}
		}
	}
	return m
}
func (m *Matrix) mask(x, y, w, h int, contains func(int, int) bool, style any, o Object) *Matrix {
	cell := resolvePixel(style, m.Base)
	fill := b(o, "fill", true)
	inside := func(i, j int) bool { return i >= 0 && j >= 0 && i < w && j < h && contains(i, j) }
	for j := maxInt(0, -y); j < minInt(h, m.Rows-y); j++ {
		for i := maxInt(0, -x); i < minInt(w, m.Columns-x); i++ {
			if inside(i, j) && (fill || !inside(i-1, j) || !inside(i+1, j) || !inside(i, j-1) || !inside(i, j+1)) {
				m.plot(x+i, y+j, cell)
			}
		}
	}
	return m
}
func minInt(a, b int) int {
	if a < b {
		return a
	}
	return b
}
func maxInt(a, b int) int {
	if a > b {
		return a
	}
	return b
}
func checkExtent(w, h int) {
	if w < 1 || h < 1 || w > 4096 || h > 4096 {
		fail("shape dimensions must be 1..4096")
	}
}
func (m *Matrix) ellipse(x, y, w, h int, style any, o Object) *Matrix {
	checkExtent(w, h)
	ww, hh := int64(w)*int64(w), int64(h)*int64(h)
	return m.mask(x, y, w, h, func(i, j int) bool { dx, dy := int64(2*i+1-w), int64(2*j+1-h); return dx*dx*hh+dy*dy*ww <= ww*hh }, style, o)
}
func (m *Matrix) roundRect(x, y, w, h, r int, style any, o Object) *Matrix {
	checkExtent(w, h)
	if r < 0 {
		fail("radius must be non-negative")
	}
	corner := minInt(r, minInt(w, h)/2)
	return m.mask(x, y, w, h, func(i, j int) bool {
		qi, qj := minInt(i, w-1-i), minInt(j, h-1-j)
		if qi >= corner || qj >= corner {
			return true
		}
		dx, dy := 2*qi+1-2*corner, 2*qj+1-2*corner
		return dx*dx+dy*dy <= 4*corner*corner
	}, style, o)
}
func (m *Matrix) circle(cx, cy, r int, style any, o Object) *Matrix {
	if r < 0 {
		fail("radius must be non-negative")
	}
	return m.ellipse(cx-r, cy-r, 2*r+1, 2*r+1, style, o)
}
func (m *Matrix) arc(cx, cy, r int, start, end float64, style any, o Object) *Matrix {
	thickness := positive(n(o, "thickness", 1))
	if r < 0 || r > 4096 || thickness > 4096 {
		fail("invalid arc radius/thickness")
	}
	finite(start)
	finite(end)
	cell := resolvePixel(style, m.Base)
	sweep := end - start
	if sweep <= 0 {
		return m
	}
	direction := func(degrees float64) (int64, int64) {
		rad := degrees * math.Pi / 180
		return int64(round(math.Sin(rad) * ArcDirectionScale)), -int64(round(math.Cos(rad) * ArcDirectionScale))
	}
	sx, sy := direction(start)
	ex, ey := direction(end)
	outer, hole := r*r+r, r-thickness
	inner := -1
	if hole >= 0 {
		inner = hole*hole + hole
	}
	for y := maxInt(0, cy-r); y <= minInt(m.Rows-1, cy+r); y++ {
		for x := maxInt(0, cx-r); x <= minInt(m.Columns-1, cx+r); x++ {
			dx, dy := x-cx, y-cy
			after, before := sx*int64(dy)-sy*int64(dx) >= 0, int64(dx)*ey-int64(dy)*ex >= 0
			segment := sweep >= 360 || (sweep < 180 && after && before) || (sweep >= 180 && (after || before))
			d := dx*dx + dy*dy
			if d <= outer && d > inner && segment {
				m.plot(x, y, cell)
			}
		}
	}
	return m
}
func (m *Matrix) polygon(points []any, style any, o Object) *Matrix {
	if len(points) < 3 {
		fail("polygon needs at least 3 points")
	}
	xy := make([][2]int, len(points))
	for i, p := range points {
		a := array(p)
		if len(a) != 2 {
			fail("polygon points must be pairs")
		}
		x, y := integer(number(a[0])), integer(number(a[1]))
		if math.Abs(float64(x)) > RasterMaxCoordinate || math.Abs(float64(y)) > RasterMaxCoordinate {
			fail("polygon coordinates out of range")
		}
		xy[i] = [2]int{x, y}
	}
	cell := resolvePixel(style, m.Base)
	left, right, top, bottom := xy[0][0], xy[0][0], xy[0][1], xy[0][1]
	for i, a := range xy {
		b := xy[(i+1)%len(xy)]
		rasterLine(a[0], a[1], b[0], b[1], func(x, y int) { m.plot(x, y, cell) })
		left = minInt(left, a[0])
		right = maxInt(right, a[0])
		top = minInt(top, a[1])
		bottom = maxInt(bottom, a[1])
	}
	if !b(o, "fill", true) {
		return m
	}
	for y := maxInt(0, top); y <= minInt(m.Rows-1, bottom); y++ {
		cy := 2*y + 1
		for x := maxInt(0, left); x <= minInt(m.Columns-1, right); x++ {
			rightCount, through := 0, 0
			for i, a := range xy {
				b := xy[(i+1)%len(xy)]
				if (2*a[1] <= cy) == (2*b[1] <= cy) {
					continue
				}
				dy := int64(b[1] - a[1])
				t := int64(2*a[0]-2*x-1)*dy + int64(cy-2*a[1])*int64(b[0]-a[0])
				if t == 0 {
					through++
				} else if (t > 0) == (dy > 0) {
					rightCount++
				}
			}
			if rightCount%2 == 1 || (rightCount+through)%2 == 1 {
				m.plot(x, y, cell)
			}
		}
	}
	return m
}
func (m *Matrix) floodFill(x, y int, style any, o Object) *Matrix {
	if !m.contains(x, y) {
		return m
	}
	target, cell := m.get(x, y), resolvePixel(style, m.Base)
	offsets := [][2]int{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}
	if b(o, "diagonal", false) {
		offsets = append(offsets, [2]int{1, 1}, [2]int{1, -1}, [2]int{-1, 1}, [2]int{-1, -1})
	}
	visited := make([]bool, len(m.cells))
	pending := []int{m.index(x, y)}
	visited[pending[0]] = true
	for len(pending) > 0 {
		i := pending[len(pending)-1]
		pending = pending[:len(pending)-1]
		cx, cy := i%m.Columns, i/m.Columns
		m.cells[i] = cell
		for _, d := range offsets {
			xx, yy := cx+d[0], cy+d[1]
			if !m.contains(xx, yy) {
				continue
			}
			j := yy*m.Columns + xx
			if !visited[j] && reflect.DeepEqual(m.get(xx, yy), target) {
				visited[j] = true
				pending = append(pending, j)
			}
		}
	}
	return m
}
func (m *Matrix) emptyLike(c, r int) *Matrix {
	return newMatrix(c, r, Object{"default": m.DefaultStyle, "base": m.Base, "layout": m.Layout})
}
func (m *Matrix) copy() *Matrix {
	result := m.emptyLike(m.Columns, m.Rows)
	for i, c := range m.cells {
		result.cells[i] = clone(c)
	}
	return result
}
func (m *Matrix) flipX() *Matrix {
	out := m.emptyLike(m.Columns, m.Rows)
	for y := 0; y < m.Rows; y++ {
		for x := 0; x < m.Columns; x++ {
			out.cells[y*m.Columns+m.Columns-1-x] = clone(m.cells[y*m.Columns+x])
		}
	}
	return out
}
func (m *Matrix) flipY() *Matrix {
	out := m.emptyLike(m.Columns, m.Rows)
	for y := 0; y < m.Rows; y++ {
		for x := 0; x < m.Columns; x++ {
			out.cells[(m.Rows-1-y)*m.Columns+x] = clone(m.cells[y*m.Columns+x])
		}
	}
	return out
}
func (m *Matrix) rotate90(turns int) *Matrix {
	out := m.copy()
	for turn := ((turns % 4) + 4) % 4; turn > 0; turn-- {
		previous := out
		out = previous.emptyLike(previous.Rows, previous.Columns)
		for y := 0; y < previous.Rows; y++ {
			for x := 0; x < previous.Columns; x++ {
				out.cells[x*previous.Rows+previous.Rows-1-y] = clone(previous.cells[y*previous.Columns+x])
			}
		}
	}
	return out
}
func (m *Matrix) crop(x, y, w, h int) *Matrix {
	out := m.emptyLike(w, h)
	for r := 0; r < h; r++ {
		for c := 0; c < w; c++ {
			if m.contains(x+c, y+r) {
				out.cells[r*w+c] = clone(m.cells[(y+r)*m.Columns+x+c])
			}
		}
	}
	return out
}
func protocolPixel(v any) Object {
	if v == Empty {
		return Object{"empty": true}
	}
	return clone(v).(Object)
}
func (m *Matrix) toRoot() Object {
	type group struct {
		pixel       Object
		coordinates []any
	}
	groups := []*group{}
	byKey := map[string]*group{}
	defaultKey := batchKey(protocolPixel(m.DefaultStyle))
	for i, cell := range m.cells {
		if cell == nil {
			continue
		}
		pixel := protocolPixel(cell)
		k := batchKey(pixel)
		if k == defaultKey {
			continue
		}
		g := byKey[k]
		if g == nil {
			g = &group{pixel, []any{}}
			byKey[k] = g
			groups = append(groups, g)
		}
		g.coordinates = append(g.coordinates, []any{i / m.Columns, i % m.Columns})
	}
	pixels := make([]any, m.Rows)
	for y := range pixels {
		pixels[y] = make([]any, m.Columns)
	}
	batch := []any{}
	for _, g := range groups {
		if len(g.coordinates) >= 2 {
			batch = append(batch, Object{"pixel": g.pixel, "coordinates": g.coordinates})
		} else {
			p := g.coordinates[0].([]any)
			pixels[p[0].(int)].([]any)[p[1].(int)] = g.pixel
		}
	}
	out := Object{"pixels": pixels, "default": protocolPixel(m.DefaultStyle), "layout": clone(m.Layout)}
	if len(batch) > 0 {
		out["batch"] = batch
	}
	return out
}
func (m *Matrix) toPart() Object { return Object{"content": m.toRoot()} }

func NewMatrix(columns, rows int, options ...Object) (*Matrix, error) {
	return safe(func() *Matrix { return newMatrix(columns, rows, opt(options)) })
}
func (m *Matrix) Contains(x, y int) bool    { return m.contains(x, y) }
func (m *Matrix) Get(x, y int) (any, error) { return safe(func() any { return clone(m.get(x, y)) }) }
func (m *Matrix) IsSet(x, y int) (bool, error) {
	return safe(func() bool { return m.cells[m.index(x, y)] != nil })
}
func (m *Matrix) Set(x, y int, style any) (*Matrix, error) {
	return safe(func() *Matrix { return m.set(x, y, style) })
}
func (m *Matrix) Fill(style any) (*Matrix, error) {
	return safe(func() *Matrix { return m.fill(style) })
}
func (m *Matrix) Clear() *Matrix { return m.fill(nil) }
func (m *Matrix) Blit(other *Matrix, x, y int) (*Matrix, error) {
	return safe(func() *Matrix { return m.blit(other, x, y) })
}
func (m *Matrix) Line(x0, y0, x1, y1 int, style any) (*Matrix, error) {
	return safe(func() *Matrix { return m.line(x0, y0, x1, y1, style) })
}
func (m *Matrix) Rect(x, y, w, h int, style any, options ...Object) (*Matrix, error) {
	return safe(func() *Matrix { return m.rect(x, y, w, h, style, opt(options)) })
}
func (m *Matrix) RoundRect(x, y, w, h, r int, style any, options ...Object) (*Matrix, error) {
	return safe(func() *Matrix { return m.roundRect(x, y, w, h, r, style, opt(options)) })
}
func (m *Matrix) Ellipse(x, y, w, h int, style any, options ...Object) (*Matrix, error) {
	return safe(func() *Matrix { return m.ellipse(x, y, w, h, style, opt(options)) })
}
func (m *Matrix) Circle(x, y, r int, style any, options ...Object) (*Matrix, error) {
	return safe(func() *Matrix { return m.circle(x, y, r, style, opt(options)) })
}
func (m *Matrix) Arc(x, y, r int, start, end float64, style any, options ...Object) (*Matrix, error) {
	return safe(func() *Matrix { return m.arc(x, y, r, start, end, style, opt(options)) })
}
func (m *Matrix) Polygon(points []any, style any, options ...Object) (*Matrix, error) {
	return safe(func() *Matrix { return m.polygon(points, style, opt(options)) })
}
func (m *Matrix) FloodFill(x, y int, style any, options ...Object) (*Matrix, error) {
	return safe(func() *Matrix { return m.floodFill(x, y, style, opt(options)) })
}
func (m *Matrix) Clone() *Matrix { return m.copy() }
func (m *Matrix) FlipX() *Matrix { return m.flipX() }
func (m *Matrix) FlipY() *Matrix { return m.flipY() }
func (m *Matrix) Rotate90(turns ...int) *Matrix {
	t := 1
	if len(turns) > 0 {
		t = turns[0]
	}
	return m.rotate90(t)
}
func (m *Matrix) Crop(x, y, w, h int) (*Matrix, error) {
	return safe(func() *Matrix { return m.crop(x, y, w, h) })
}
func (m *Matrix) ToRoot() RootMatrix { return m.toRoot() }
func (m *Matrix) ToPart() MatrixPart { return m.toPart() }
