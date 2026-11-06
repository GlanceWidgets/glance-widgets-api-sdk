package glance

const OffPixelOpacity = .3
const RootRingOpacityThreshold = .4
const DefaultThemeName = "lcd"

var StatusColors = Object{"good": "#34C759", "warn": "#FF9F0A", "bad": "#FF453A", "neutral": "#8E8E93"}

func defineTheme(name string, colors []string, corner float64, gap string, rotation, halo float64) Object {
	palette := Object{"on": colors[0], "off": colors[1], "dim": colors[2], "accent": colors[3]}
	return Object{"name": name, "default": Object{"color": colors[1], "opacity": .3, "cornerRadius": corner, "rotation": rotation, "halo": 0.}, "base": Object{"color": colors[0], "opacity": 1., "cornerRadius": corner, "rotation": rotation, "halo": halo}, "layout": Object{"gapMode": gap, "edgeInsetRatio": 0., "cellAspectRatio": 1.}, "palette": palette}
}

var Themes = map[string]Object{
	"led":     defineTheme("led", []string{"#FF4D2E", "#3A1F1B", "#A33A26", "#FFD23F"}, .5, "far", 0, .6),
	"lcd":     defineTheme("lcd", []string{"#9BFF6B", "#27402B", "#5E9B4A", "#E6FF8A"}, 0, "near", 0, 0),
	"dot":     defineTheme("dot", []string{"#F2F2F7", "#48484A", "#8E8E93", "#0A84FF"}, .5, "extraFar", 0, 0),
	"diamond": defineTheme("diamond", []string{"#5AC8FA", "#1C3A4A", "#3D7C99", "#FF9F0A"}, .1, "extraFar", 45, 0),
	"soft":    defineTheme("soft", []string{"#FFE2B8", "#4A3F38", "#B59A82", "#FF7A59"}, .3, "default", 0, .25),
}

func getTheme(name string) Object {
	if theme, ok := Themes[name]; ok {
		return clone(theme).(Object)
	}
	fail("unknown theme %q", name)
	return nil
}
func themeOptions(theme, o Object) Object {
	def := o["default"]
	if def == nil {
		def = theme["default"]
	}
	return Object{"default": clone(def), "base": join(object(theme["base"]), object(o["base"])), "layout": join(object(theme["layout"]), object(o["layout"]))}
}
func resolveTheme(value any) Object {
	if value == nil {
		return getTheme("lcd")
	}
	if name, ok := value.(string); ok {
		return getTheme(name)
	}
	return object(value)
}
func GetTheme(name string) (Object, error) { return safe(func() Object { return getTheme(name) }) }
func ThemeOptions(theme Object, overrides ...Object) (Object, error) {
	return safe(func() Object { return themeOptions(theme, opt(overrides)) })
}
