package glance

import (
	"encoding/json"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"testing"
)

func must[T any](v T, e error) T {
	if e != nil {
		panic(e)
	}
	return v
}
func buildGoldenMatrix(spec Object) *Matrix {
	options := object(spec["options"])
	if has(spec, "theme") {
		options = must(ThemeOptions(must(GetTheme(s(spec, "theme", ""))), options))
	}
	m := must(NewMatrix(positive(n(spec, "columns", 0)), positive(n(spec, "rows", 0)), options))
	for _, operation := range array(spec["ops"]) {
		op := array(operation)
		name := op[0].(string)
		a := op[1:]
		i := func(k int) int { return integer(number(a[k])) }
		f := func(k int) float64 { return number(a[k]) }
		o := func(k int) Object {
			if len(a) <= k {
				return Object{}
			}
			return object(a[k])
		}
		switch name {
		case "set":
			m = must(m.Set(i(0), i(1), a[2]))
		case "fill":
			m = must(m.Fill(a[0]))
		case "clear":
			m = m.Clear()
		case "line":
			m = must(m.Line(i(0), i(1), i(2), i(3), a[4]))
		case "rect":
			m = must(m.Rect(i(0), i(1), i(2), i(3), a[4], o(5)))
		case "roundRect":
			m = must(m.RoundRect(i(0), i(1), i(2), i(3), i(4), a[5], o(6)))
		case "ellipse":
			m = must(m.Ellipse(i(0), i(1), i(2), i(3), a[4], o(5)))
		case "circle":
			m = must(m.Circle(i(0), i(1), i(2), a[3], o(4)))
		case "arc":
			m = must(m.Arc(i(0), i(1), i(2), f(3), f(4), a[5], o(6)))
		case "polygon":
			m = must(m.Polygon(array(a[0]), a[1], o(2)))
		case "floodFill":
			m = must(m.FloodFill(i(0), i(1), a[2], o(3)))
		case "text":
			m = must(m.Text(a[0].(string), i(1), i(2), o(3)))
		case "blit":
			m = must(m.Blit(buildGoldenMatrix(object(a[0])), i(1), i(2)))
		case "flipX":
			m = m.FlipX()
		case "flipY":
			m = m.FlipY()
		case "rotate90":
			turns := 1
			if len(a) > 0 {
				turns = i(0)
			}
			m = m.Rotate90(turns)
		case "crop":
			m = must(m.Crop(i(0), i(1), i(2), i(3)))
		default:
			fail("unknown golden matrix op %s", name)
		}
	}
	return m
}
func asciiPicture(m *Matrix) []string {
	legend := map[string]string{}
	alphabet := "#ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	rows := []string{}
	for y := 0; y < m.Rows; y++ {
		line := ""
		for x := 0; x < m.Columns; x++ {
			style := must(m.Get(x, y))
			if !must(m.IsSet(x, y)) {
				line += "."
			} else if style == Empty {
				line += "_"
			} else {
				k := key(style)
				char, ok := legend[k]
				if !ok {
					if len(legend) >= len(alphabet) {
						fail("too many ASCII styles")
					}
					char = string(alphabet[len(legend)])
					legend[k] = char
				}
				line += char
			}
		}
		rows = append(rows, line)
	}
	return rows
}
func runGoldenCall(call Object) (any, error) {
	return safe(func() any {
		name := s(call, "fn", "")
		args := array(call["args"])
		a := func(i int) any {
			if len(args) <= i {
				return nil
			}
			return args[i]
		}
		o := func(i int) Object { return object(a(i)) }
		switch name {
		case "matrix":
			spec := o(0)
			m := buildGoldenMatrix(spec)
			switch s(spec, "export", "root") {
			case "ascii":
				return asciiPicture(m)
			case "part":
				return m.ToPart()
			default:
				return m.ToRoot()
			}
		case "place":
			return must(Place(buildGoldenMatrix(o(0)).ToPart(), o(1)))
		case "merge":
			parts := []Object{}
			for _, v := range args {
				parts = append(parts, object(v))
			}
			return must(Merge(parts...))
		case "toResponse":
			return must(ToResponse(o(0)))
		case "validate":
			if err := Validate(o(0)); err != nil {
				panic(err)
			}
			return nil
		case "normalizeColor":
			return must(NormalizeColor(a(0).(string)))
		case "withAlpha":
			return must(WithAlpha(a(0).(string), number(a(1))))
		case "mixColors":
			return must(MixColors(a(0).(string), a(1).(string), number(a(2))))
		case "sign":
			return must(Sign(a(0), a(1).(string)))
		case "verifySignature":
			return VerifySignature(o(0), a(1).(string), o(2))
		case "theme":
			return must(GetTheme(a(0).(string)))
		case "iconGlyph":
			return must(IconGlyph(a(0).(string)))
		case "text":
			return must(Text(o(0)))
		case "line":
			return must(Line(o(0)))
		case "horizontalLine":
			return must(HorizontalLine(o(0)))
		case "verticalLine":
			return must(VerticalLine(o(0)))
		case "shape":
			return must(Shape(o(0)))
		case "image":
			return must(Image(o(0)))
		case "icon":
			return must(Icon(o(0)))
		case "curve":
			return must(Curve(o(0)))
		case "resolveFrame":
			return must(ResolveFrame(o(0), canvasOf(a(1))))
		case "fitMatrix":
			return must(FitMatrix(rectOf(a(0)), o(1), o(2)))
		case "solveGrid":
			return must(SolveGrid(o(0)))
		case "boxFrame":
			spec := o(0)
			var box *Box
			if has(spec, "canvas") {
				box = CanvasBox(canvasOf(spec["canvas"]))
			} else {
				box = CanvasBox()
			}
			for _, operation := range array(spec["ops"]) {
				op := array(operation)
				args := op[1:]
				switch op[0] {
				case "sub":
					box = must(box.Sub(number(args[0]), number(args[1]), number(args[2]), number(args[3])))
				case "inset":
					box = must(box.Inset(args[0]))
				case "insetPt":
					box = must(box.InsetPt(args[0]))
				case "rows", "columns":
					box = must(box.Split(op[0].(string), args[0], object(args[1])))[integer(number(args[2]))]
				default:
					fail("unknown golden box op")
				}
			}
			return must(box.ToFrame())
		case "measureText":
			return must(MeasureText(a(0).(string), a(1), o(2)))
		case "textMatrix":
			return asciiPicture(must(TextMatrix(a(0).(string), o(1))))
		case "pixelate":
			image := o(0)
			data := []byte{}
			for _, v := range array(image["data"]) {
				data = append(data, byte(number(v)))
			}
			m := must(Pixelate(RGBAImage{positive(n(image, "width", 0)), positive(n(image, "height", 0)), data}, o(1)))
			colors := []string{}
			seen := map[string]bool{}
			for y := 0; y < m.Rows; y++ {
				for x := 0; x < m.Columns; x++ {
					style := must(m.Get(x, y))
					if must(m.IsSet(x, y)) && style != Empty {
						color := s(object(style), "color", "")
						if !seen[color] {
							colors = append(colors, color)
							seen[color] = true
						}
					}
				}
			}
			return Object{"picture": asciiPicture(m), "colors": colors}
		case "progressBar":
			return must(ProgressBar(number(a(0)), o(1)))
		case "ringProgress":
			return must(RingProgress(number(a(0)), o(1)))
		case "battery":
			return must(Battery(number(a(0)), o(1)))
		case "signal":
			return must(Signal(number(a(0)), o(1)))
		case "pixelProgressBar":
			return must(PixelProgressBar(number(a(0)), o(1)))
		case "labeledPixelProgressBar":
			return must(LabeledPixelProgressBar(number(a(0)), o(1)))
		case "pixelRingProgress":
			return must(PixelRingProgress(number(a(0)), o(1)))
		case "labeledPixelRingProgress":
			return must(LabeledPixelRingProgress(number(a(0)), o(1)))
		case "statusStrip":
			return must(StatusStrip(array(a(0)), o(1)))
		case "labeledStatusStrip":
			return must(LabeledStatusStrip(array(a(0)), o(1)))
		case "pixelTextPanel":
			return must(PixelTextPanel(a(0).(string), o(1)))
		case "pixelTextRoot":
			return must(PixelTextRoot(a(0).(string), o(1)))
		case "heatmap":
			return must(Heatmap(a(0), o(1)))
		case "calendarHeatmap":
			return must(CalendarHeatmap(array(a(0)), o(1)))
		case "labeledCalendarHeatmap":
			return must(LabeledCalendarHeatmap(array(a(0)), o(1)))
		case "barChart":
			return must(BarChart(goldenNumbers(a(0)), o(1)))
		case "pixelBarChart":
			return must(PixelBarChart(goldenNumbers(a(0)), o(1)))
		case "lineChart":
			return must(LineChart(array(a(0)), o(1)))
		case "areaChart":
			return must(AreaChart(array(a(0)), o(1)))
		case "sparkline":
			return must(Sparkline(goldenNumbers(a(0)), o(1)))
		case "stat":
			return must(Stat(a(0), o(1)))
		default:
			fail("unknown golden function %s", name)
		}
		return nil
	})
}

func goldenNumbers(value any) []float64 {
	values := array(value)
	result := make([]float64, len(values))
	for i, value := range values {
		result[i] = number(value)
	}
	return result
}
func normalized(v any) any {
	bytes, err := json.Marshal(v)
	if err != nil {
		panic(err)
	}
	var out any
	if err = json.Unmarshal(bytes, &out); err != nil {
		panic(err)
	}
	return out
}
func difference(actual, expected any, path string) string {
	if a, ok := actual.(float64); ok {
		if e, ok := expected.(float64); ok {
			if math.Abs(a-e) <= 1e-9*math.Max(1, math.Abs(e)) {
				return ""
			}
			return fmt.Sprintf("%s: %v != %v", path, a, e)
		}
	}
	if a, ok := actual.([]any); ok {
		if e, ok := expected.([]any); ok {
			if len(a) != len(e) {
				return fmt.Sprintf("%s: length %d != %d", path, len(a), len(e))
			}
			for i, v := range a {
				if diff := difference(v, e[i], fmt.Sprintf("%s[%d]", path, i)); diff != "" {
					return diff
				}
			}
			return ""
		}
	}
	if a, ok := actual.(map[string]any); ok {
		if e, ok := expected.(map[string]any); ok {
			keys := []string{}
			for k := range a {
				keys = append(keys, k)
			}
			for k := range e {
				if !has(a, k) {
					return path + "." + k + ": missing"
				}
			}
			sort.Strings(keys)
			for _, k := range keys {
				if !has(e, k) {
					return path + "." + k + ": unexpected"
				}
				if diff := difference(a[k], e[k], path+"."+k); diff != "" {
					return diff
				}
			}
			return ""
		}
	}
	if fmt.Sprintf("%T:%v", actual, actual) == fmt.Sprintf("%T:%v", expected, expected) {
		return ""
	}
	return fmt.Sprintf("%s: %v != %v", path, actual, expected)
}
func TestGolden(t *testing.T) {
	files, err := filepath.Glob("../../spec/golden/*.json")
	if err != nil || len(files) != 27 {
		t.Fatalf("need 27 golden files: %v", err)
	}
	total := 0
	for _, file := range files {
		data, err := os.ReadFile(file)
		if err != nil {
			t.Fatal(err)
		}
		cases, err := decodeGoldenCases(data)
		if err != nil {
			t.Fatal(err)
		}
		for _, c := range cases {
			total++
			c := c
			t.Run(filepath.Base(file)+"/"+s(c, "name", ""), func(t *testing.T) {
				actual, err := runGoldenCall(object(c["call"]))
				if err != nil && strings.Contains(err.Error(), "unknown golden") {
					t.Fatal(err)
				}
				if b(c, "throws", false) {
					if err == nil {
						t.Fatal("expected an error")
					}
					return
				}
				if err != nil {
					t.Fatal(err)
				}
				if diff := difference(normalized(actual), c["expected"], "$"); diff != "" {
					t.Fatal(diff)
				}
			})
		}
	}
	t.Logf("Go golden: %d cases from %d files", total, len(files))
}
