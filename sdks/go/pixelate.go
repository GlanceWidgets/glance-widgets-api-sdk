package glance

const DefaultAlphaThreshold = 128

type RGBAImage struct {
	Width, Height int
	Data          []byte
}

func pixelate(image, o Object) *Matrix {
	width, height := positive(n(image, "width", 0)), positive(n(image, "height", 0))
	data := array(image["data"])
	if len(data) != width*height*4 {
		fail("RGBA data length does not match image size")
	}
	if s(o, "dither", "none") != "none" {
		fail("unsupported dither")
	}
	type entry struct {
		color string
		rgba  RGBA
	}
	palette := []entry{}
	for _, v := range array(o["palette"]) {
		color := v.(string)
		palette = append(palette, entry{normalizeColor(color), parseColor(color)})
	}
	if has(o, "palette") && len(palette) == 0 {
		fail("palette must not be empty")
	}
	m := newMatrix(positive(n(o, "columns", 0)), positive(n(o, "rows", 0)), object(o["matrix"]))
	divRound := func(n, d int64) int64 { return (2*n + d) / (2 * d) }
	for row := 0; row < m.Rows; row++ {
		top := row * height / m.Rows
		bottom := maxInt(top+1, (row+1)*height/m.Rows)
		for col := 0; col < m.Columns; col++ {
			left := col * width / m.Columns
			right := maxInt(left+1, (col+1)*width/m.Columns)
			var alpha, r, g, b int64
			for y := top; y < bottom; y++ {
				for x := left; x < right; x++ {
					i := (y*width + x) * 4
					a := int64(number(data[i+3]))
					alpha += a
					r += int64(number(data[i])) * a
					g += int64(number(data[i+1])) * a
					b += int64(number(data[i+2])) * a
				}
			}
			if float64(divRound(alpha, int64((bottom-top)*(right-left)))) < n(o, "alphaThreshold", 128) {
				continue
			}
			average := RGBA{0, 0, 0, 255}
			if alpha > 0 {
				average.R = float64(divRound(r, alpha))
				average.G = float64(divRound(g, alpha))
				average.B = float64(divRound(b, alpha))
			}
			color := formatColor(average)
			best := 1e100
			for _, p := range palette {
				dr, dg, db := p.rgba.R-average.R, p.rgba.G-average.G, p.rgba.B-average.B
				distance := dr*dr + dg*dg + db*db
				if distance < best {
					best = distance
					color = p.color
				}
			}
			m.set(col, row, color)
		}
	}
	return m
}
func Pixelate(image RGBAImage, options Object) (*Matrix, error) {
	return safe(func() *Matrix {
		data := make([]any, len(image.Data))
		for i, v := range image.Data {
			data[i] = float64(v)
		}
		return pixelate(Object{"width": image.Width, "height": image.Height, "data": data}, options)
	})
}
