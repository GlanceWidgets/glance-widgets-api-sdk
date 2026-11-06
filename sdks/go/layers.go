package glance

import (
	"sort"
	"strings"
)

var TextDefaults = Object{"color": "#FFFFFF", "textOpacity": 1., "opacity": 0., "fontSizeRatio": .12, "fontWeight": "400", "fontFamily": "system", "letterSpacing": 0., "horizontalAlignment": "left", "verticalAlignment": "top", "autoWrap": false, "paddingRatio": 0.}
var LineDefaults = Object{"color": "#FFFFFF", "opacity": 1., "strokeWidth": 2., "pattern": "solid", "cap": "butt"}
var ShapeDefaults = Object{"color": "#2A63D4", "opacity": 1., "cornerRadiusRatio": 0.}
var IconDefaults = Object{"color": "#FFFFFF", "textOpacity": 1.}
var ImageDefaults = Object{"opacity": 1., "cornerRadiusRatio": 0.}
var CurveDefaults = Object{"color": "#6BA7FF", "opacity": 1., "strokeWidth": 2., "lineCap": "round", "lineJoin": "round", "fillOpacity": 0., "plotPadding": 0.}
var icons = object(loadData("data/icons.json")["icons"])

const DefaultIconName = "ph_activity"

func iconGlyph(name string) Object {
	entry, ok := icons[name]
	if !ok {
		fail("unknown icon %q", name)
	}
	return join(Object{"icon": name}, object(entry))
}
func HasIcon(name string) bool { _, ok := icons[name]; return ok }
func IconNames() []string {
	names := make([]string, 0, len(icons))
	for k := range icons {
		names = append(names, k)
	}
	sort.Strings(names)
	return names
}
func IconGlyph(name string) (Object, error) { return safe(func() Object { return iconGlyph(name) }) }
func normalizeStyle(style Object) Object {
	out := join(style)
	for k, v := range out {
		if k == "color" || strings.HasSuffix(k, "Color") {
			if color, ok := v.(string); ok {
				out[k] = normalizeColor(color)
			}
		} else if k == "gradientColors" {
			a := []any{}
			for _, v := range array(v) {
				a = append(a, normalizeColor(v.(string)))
			}
			out[k] = a
		}
	}
	return out
}
func layerBase(kind string, o Object) Object {
	node := Object{"frame": frameOf(o["frame"])}
	for _, k := range []string{"zIndex", "rotation", "transform"} {
		if has(o, k) {
			node[k] = clone(o[k])
		}
	}
	path := s(o, "dataPath", layerPath(s(o, "id", "")))
	return Object{"layers": []any{Object{"id": layerID(s(o, "id", "")), "type": kind, "dataPath": path}}, "data": nestAtPath(path, node)}
}
func finishLayer(base, fields Object) Object {
	declaration := object(array(base["layers"])[0])
	path := s(declaration, "dataPath", "")
	node := join(object(resolveDataPath(base["data"], path)), fields)
	base["data"] = nestAtPath(path, node)
	return base
}
func text(o Object) Object {
	return finishLayer(layerBase("text", o), Object{"content": Object{"text": getValue(o, "text", "")}, "style": normalizeStyle(join(TextDefaults, object(o["style"])))})
}
func line(o Object) Object {
	return finishLayer(layerBase("line", o), Object{"content": Object{"direction": o["direction"]}, "style": normalizeStyle(join(LineDefaults, object(o["style"])))})
}
func required(o Object, k string) float64 {
	if !set(o, k) {
		fail("line option %q is required", k)
	}
	return n(o, k, 0)
}
func axisLine(o Object, vertical bool) Object {
	at, from, to := required(o, "at"), required(o, "from"), required(o, "to")
	length := to - from
	if length < .02 {
		fail("line length must be at least .02")
	}
	var frame *Box
	direction := "horizontal"
	if vertical {
		frame = newBox(at, from, 0, length, nil)
		direction = "vertical"
	} else {
		frame = newBox(from, at, length, 0, nil)
	}
	return line(join(o, Object{"frame": frame, "direction": direction}))
}
func shape(o Object) Object {
	if box, ok := o["frame"].(*Box); ok && box.Canvas != nil {
		r := box.toPt()
		if r.Width < 24 || r.Height < 24 {
			fail("shape below App minimum 24pt")
		}
	}
	style := join(ShapeDefaults, object(o["style"]))
	if !set(style, "backgroundColor") {
		style["backgroundColor"] = style["color"]
	}
	base := layerBase("shape", o)

	return finishLayer(base, Object{"content": Object{}, "style": normalizeStyle(style)})
}
func icon(o Object) Object {
	count := 0
	for _, k := range []string{"name", "glyph", "url"} {
		if has(o, k) {
			count++
		}
	}
	if count != 1 {
		fail("icon needs exactly one source")
	}
	var content Object
	var glyph Object
	if set(o, "glyph") {
		glyph = object(o["glyph"])
	} else if has(o, "name") {
		glyph = iconGlyph(s(o, "name", ""))
	}
	if glyph != nil {
		content = Object{}
		for _, k := range []string{"icon", "iconCodePoint", "iconFont"} {
			if v, ok := glyph[k]; ok {
				content[k] = v
			}
		}
	} else if has(o, "url") {
		content = Object{"url": o["url"]}
	} else {
		content = Object{}
	}
	return finishLayer(layerBase("icon", o), Object{"content": content, "style": normalizeStyle(join(IconDefaults, object(o["style"])))})
}
func image(o Object) Object {
	if !has(o, "url") {
		fail("image needs url")
	}
	content := Object{}
	if has(o, "url") {
		content = Object{"url": o["url"]}
	}
	return finishLayer(layerBase("image", o), Object{"content": content, "style": normalizeStyle(join(ImageDefaults, object(o["style"])))})
}
func curve(o Object) Object {
	data := object(o["data"])
	mode := s(data, "mode", "")
	modeSet := set(data, "mode")
	function := mode == "function" || (!has(data, "mode") && has(data, "expression"))
	if function && (!has(data, "domain") || !has(data, "yDomain")) {
		fail("function curve needs domain and yDomain")
	}
	if len(array(data["points"])) > 1024 || len(array(data["markers"])) > 32 {
		fail("curve point/marker limit exceeded")
	}
	for _, m := range array(data["markers"]) {
		at := object(object(m)["at"])
		for _, v := range array(at["offsetPt"]) {
			if number(v) > 64 || number(v) < -64 {
				fail("marker offset exceeds 64pt")
			}
		}
	}
	if !modeSet {
		mode = "samples"
		if has(data, "expression") {
			mode = "function"
		}
	}
	content := join(data, Object{"mode": mode, "interpolation": s(data, "interpolation", "monotone")})
	style := join(CurveDefaults, object(o["style"]))
	if !set(style, "fillColor") {
		style["fillColor"] = style["color"]
	}
	return finishLayer(layerBase("curve", o), Object{"content": content, "style": normalizeStyle(style)})
}
func Text(o Object) (Object, error) { return safe(func() Object { return text(o) }) }
func Line(o Object) (Object, error) { return safe(func() Object { return line(o) }) }
func HorizontalLine(o Object) (Object, error) {
	return safe(func() Object { return axisLine(o, false) })
}
func VerticalLine(o Object) (Object, error) { return safe(func() Object { return axisLine(o, true) }) }
func Shape(o Object) (Object, error)        { return safe(func() Object { return shape(o) }) }
func Icon(o Object) (Object, error)         { return safe(func() Object { return icon(o) }) }
func Image(o Object) (Object, error)        { return safe(func() Object { return image(o) }) }
func Curve(o Object) (Object, error)        { return safe(func() Object { return curve(o) }) }

func getValue(o Object, k string, fallback any) any {
	if v, ok := o[k]; ok && v != nil {
		return v
	}
	return fallback
}
