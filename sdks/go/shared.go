package glance

import (
	"math"
	"math/big"
	"regexp"
	"strings"
)

var textAdvances = loadData("data/text-advances.json")
var identifierPattern = regexp.MustCompile(`^[A-Za-z_][A-Za-z0-9_]*$`)

func resolveArea(value any, c Canvas) *Box {
	if value == nil {
		return canvasBox(&c)
	}
	if box, ok := value.(*Box); ok {
		return newBox(box.Left, box.Top, box.Width, box.Height, &c)
	}
	return boxFromFrame(object(value), c)
}
func emittedRegion(box *Box, c Canvas) (Object, Rect) {
	frame := box.toFrame()
	return frame, resolveFrame(frame, c)
}
func regionOf(o Object, what string) *Rect {
	canvas, frame := o["canvas"], o["frame"]
	if canvas == nil {
		if box, ok := frame.(*Box); ok && box.Canvas != nil {
			r := box.toPt()
			return &r
		}
		if frame != nil {
			fail("%s: frame needs canvas size", what)
		}
		return nil
	}
	c := canvasOf(canvas)
	_, r := emittedRegion(resolveArea(frame, c), c)
	return &r
}
func planGrid(region *Rect, size, theme Object, what string) Object {
	if region == nil {
		if size["columns"] == nil || size["rows"] == nil {
			fail("%s: grid without region needs both counts", what)
		}
		return join(size, Object{"layout": join(object(theme["layout"]))})
	}
	return solveGrid(join(size, Object{"region": *region, "gapMode": object(theme["layout"])["gapMode"], "clampAspect": true}))
}
func pixelLooks(theme, colors Object) (any, any) {
	off := theme["default"]
	if color, ok := colors["off"]; ok {
		if color == nil {
			off = Empty
		} else if text, isString := color.(string); !isString || text != "" {
			off = join(object(theme["default"]), Object{"color": color})
		}
	}
	on := colors["on"]
	if on == nil {
		on = object(theme["palette"])["on"]
	}
	return on, off
}
func textAdvanceEm(content string) float64 {
	total := 0.
	ascii, extra := array(textAdvances["ascii"]), object(textAdvances["extra"])
	for _, r := range content {
		width := 1000.
		if r >= 32 && r < 127 {
			width = number(ascii[r-32])
		} else if v, ok := extra[string(r)]; ok {
			width = number(v)
		}
		total += width + 40
	}
	return total / 1000
}
func textAdvancePt(content string, pt float64) float64     { return textAdvanceEm(content) * pt }
func estimateTextWidth(content string, pt float64) float64 { return textAdvanceEm(content)*1.05*pt + 1 }
func fontPtForWidth(content string, width float64) float64 {
	return math.Max(0, width-1) / math.Max(1e-9, textAdvanceEm(content)*1.05)
}
func textBoxHeight(pt float64) float64       { return 1.2*pt + 1 }
func fontPt(ratio float64, c Canvas) float64 { return ratio * math.Min(c.Width, c.Height) }
func fontRatio(pt float64, c Canvas) float64 { return clamp(pt/math.Min(c.Width, c.Height), .02, .28) }
func identity(o Object, def, what string) (string, string) {
	id := s(o, "id", def)
	if !has(o, "path") && !identifierPattern.MatchString(id) {
		fail("%s: non-identifier id needs explicit path", what)
	}
	return id, s(o, "path", "data."+id)
}
func formatNumber(value float64, decimals int) string {
	if decimals < 0 || decimals > 100 {
		fail("decimals must be 0..100")
	}
	if math.IsNaN(value) {
		return "NaN"
	}
	if math.IsInf(value, 0) {
		if value > 0 {
			return "Infinity"
		}
		return "-Infinity"
	}
	if math.Abs(value) >= 1e21 {
		return numberString(value)
	}
	rational := new(big.Rat).SetFloat64(math.Abs(value))
	power := new(big.Int).Exp(big.NewInt(10), big.NewInt(int64(decimals)), nil)
	numerator := new(big.Int).Mul(rational.Num(), power)
	quotient, remainder := new(big.Int), new(big.Int)
	quotient.QuoRem(numerator, rational.Denom(), remainder)
	if new(big.Int).Lsh(remainder, 1).Cmp(rational.Denom()) >= 0 {
		quotient.Add(quotient, big.NewInt(1))
	}
	digits := quotient.String()
	if len(digits) < decimals+1 {
		digits = strings.Repeat("0", decimals+1-len(digits)) + digits
	}
	if decimals > 0 {
		digits = digits[:len(digits)-decimals] + "." + digits[len(digits)-decimals:]
	}
	if value < 0 && quotient.Sign() != 0 {
		return "-" + digits
	}
	return digits
}

// decimalsOf mirrors toFixed's digits argument: NaN is 0, fractions truncate, 0..100 only.
func decimalsOf(v float64) int {
	if math.IsNaN(v) {
		return 0
	}
	if math.IsInf(v, 0) || math.Abs(v) > 100 {
		fail("decimals must be 0..100")
	}
	d := int(math.Trunc(v))
	if d < 0 {
		fail("decimals must be 0..100")
	}
	return d
}
func defaultDecimals(values ...float64) int {
	for _, v := range values {
		if math.IsInf(v, 0) || math.Trunc(v) != v {
			return 1
		}
	}
	return 0
}
func label(id string, box *Box, content, horizontal, vertical string, ratio float64, color any, opacity, z float64, weight ...string) Object {
	style := Object{"color": color, "textOpacity": opacity, "fontSizeRatio": ratio, "horizontalAlignment": horizontal, "verticalAlignment": vertical}
	if len(weight) > 0 {
		style["fontWeight"] = weight[0]
	}
	return text(Object{"id": id, "frame": box, "text": content, "zIndex": z, "style": style})
}
func PercentLabel(fraction float64) string { return formatNumber(round(fraction*100), 0) + "%" }
