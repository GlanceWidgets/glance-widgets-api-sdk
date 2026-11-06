package glance

import (
	"fmt"
	"regexp"
	"strconv"
	"strings"
)

type RGBA struct {
	R float64 `json:"r"`
	G float64 `json:"g"`
	B float64 `json:"b"`
	A float64 `json:"a"`
}

var colorPattern = regexp.MustCompile(`^#?([0-9a-fA-F]{3}|[0-9a-fA-F]{6}|[0-9a-fA-F]{8})$`)
var appColorPattern = regexp.MustCompile(`^#?([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$`)

// ECMAScript String.trim whitespace. U+0085 is deliberately excluded.
func isColorWhitespace(character rune) bool {
	switch character {
	case '\t', '\n', '\v', '\f', '\r', ' ', '\u00a0', '\u1680',
		'\u2028', '\u2029', '\u202f', '\u205f', '\u3000', '\ufeff':
		return true
	}
	return character >= '\u2000' && character <= '\u200a'
}

func parseColor(color string) RGBA {
	match := colorPattern.FindStringSubmatch(strings.TrimFunc(color, isColorWhitespace))
	if match == nil {
		fail("invalid color %q", color)
	}
	h := match[1]
	if len(h) == 3 {
		h = string([]byte{h[0], h[0], h[1], h[1], h[2], h[2]})
	}
	if len(h) == 6 {
		h = "FF" + h
	}
	channel := func(i int) float64 { v, _ := strconv.ParseInt(h[i:i+2], 16, 32); return float64(v) }
	return RGBA{channel(2), channel(4), channel(6), channel(0)}
}
func formatColor(c RGBA) string {
	ch := func(v float64) int { return int(clamp(round(finite(v)), 0, 255)) }
	rgb := fmt.Sprintf("%02X%02X%02X", ch(c.R), ch(c.G), ch(c.B))
	if ch(c.A) == 255 {
		return "#" + rgb
	}
	return fmt.Sprintf("#%02X%s", ch(c.A), rgb)
}
func normalizeColor(c string) string { return formatColor(parseColor(c)) }
func mixColors(a, b string, t float64) string {
	x, y := parseColor(a), parseColor(b)
	t = clamp(t, 0, 1)
	lerp := func(a, b float64) float64 { return a + (b-a)*t }
	return formatColor(RGBA{lerp(x.R, y.R), lerp(x.G, y.G), lerp(x.B, y.B), lerp(x.A, y.A)})
}
func IsAppColor(value any) bool             { x, ok := value.(string); return ok && appColorPattern.MatchString(x) }
func ParseColor(color string) (RGBA, error) { return safe(func() RGBA { return parseColor(color) }) }
func FormatColor(color RGBA) (string, error) {
	return safe(func() string { return formatColor(color) })
}
func NormalizeColor(color string) (string, error) {
	return safe(func() string { return normalizeColor(color) })
}
func RGB(r, g, b float64, alpha ...float64) (string, error) {
	return safe(func() string {
		a := 1.
		if len(alpha) > 0 {
			a = alpha[0]
		}
		return formatColor(RGBA{r, g, b, a * 255})
	})
}
func WithAlpha(color string, alpha float64) (string, error) {
	return safe(func() string { c := parseColor(color); c.A = alpha * 255; return formatColor(c) })
}
func MixColors(from, to string, t float64) (string, error) {
	return safe(func() string { return mixColors(from, to, t) })
}
