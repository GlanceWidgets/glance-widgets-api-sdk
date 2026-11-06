// Package glance is an SDK for generating Glance Widgets API responses.
package glance

import (
	"encoding/json"
	"fmt"
	"math"
	"reflect"
)

// Object is a JSON object. Option keys use the protocol/TS spelling.
type Object = map[string]any
type Array = []any
type Canvas struct {
	Width  float64 `json:"width"`
	Height float64 `json:"height"`
}
type Rect struct {
	X      float64 `json:"x"`
	Y      float64 `json:"y"`
	Width  float64 `json:"width"`
	Height float64 `json:"height"`
}
type Frame = Object
type RootMatrix = Object

// LayerNode holds geometry, content and style at one data path.
type LayerNode = Object

type MatrixPart = Object
type Composite = Object
type PixelStyle = Object
type MatrixOptions = Object

const (
	ProtocolVersion      = 1
	MaxResponseBytes     = 524288
	MaxMatrixCells       = 4096
	LayerIDPrefix        = "remote:"
	MaxSafeNativeInteger = 9.2e18
	Empty                = "empty"
	BatchMinOccurrences  = 2
	FrameDecimals        = 6
	RasterMaxExtent      = 4096
	RasterMaxCoordinate  = 1 << 24
	ArcDirectionScale    = 1 << 20
	CellAspectRatioMin   = .1
	CellAspectRatioMax   = 10
	EdgeInsetRatioLimit  = .45
	GapRatioMax          = 10
	FrameSizeMin         = .02
	FrameSizeMax         = 1.5
	ShapeMinSizePt       = 24
)

func fail(format string, args ...any) { panic(fmt.Errorf(format, args...)) }
func safe[T any](fn func() T) (value T, err error) {
	defer func() {
		if p := recover(); p != nil {
			if e, ok := p.(error); ok {
				err = e
			} else {
				err = fmt.Errorf("%v", p)
			}
		}
	}()
	value = fn()
	return
}
func object(v any) Object {
	if v == nil {
		return Object{}
	}
	if m, ok := v.(map[string]any); ok {
		return m
	}
	fail("expected object, got %T", v)
	return nil
}
func array(v any) []any {
	if v == nil {
		return nil
	}
	if a, ok := v.([]any); ok {
		return a
	}
	rv := reflect.ValueOf(v)
	if rv.Kind() == reflect.Array || rv.Kind() == reflect.Slice {
		a := make([]any, rv.Len())
		for i := range a {
			a[i] = rv.Index(i).Interface()
		}
		return a
	}
	fail("expected array, got %T", v)
	return nil
}
func number(v any) float64 {
	switch n := v.(type) {
	case float64:
		return n
	case float32:
		return float64(n)
	case int:
		return float64(n)
	case int8:
		return float64(n)
	case int16:
		return float64(n)
	case int64:
		return float64(n)
	case int32:
		return float64(n)
	case uint:
		return float64(n)
	case uint8:
		return float64(n)
	case uint16:
		return float64(n)
	case uint32:
		return float64(n)
	case uint64:
		return float64(n)
	case json.Number:
		f, e := n.Float64()
		if e == nil {
			return f
		}
	}
	fail("expected number, got %T", v)
	return 0
}
func n(o Object, k string, d float64) float64 {
	v, ok := o[k]
	if !ok || v == nil {
		return d
	}
	return number(v)
}
func s(o Object, k, d string) string {
	v, ok := o[k]
	if !ok || v == nil {
		return d
	}
	if x, ok := v.(string); ok {
		return x
	}
	fail("%s must be a string", k)
	return ""
}
func b(o Object, k string, d bool) bool {
	v, ok := o[k]
	if !ok || v == nil {
		return d
	}
	if x, ok := v.(bool); ok {
		return x
	}
	fail("%s must be boolean", k)
	return false
}
func has(o Object, k string) bool { _, ok := o[k]; return ok }

func mulF(a, b float64) float64 { return a * b }

// labelBoxEm is TEXT_LINE_HEIGHT*LABEL_BOX_LINES evaluated at run time (1.7999999999999998, not the
// exact constant 1.8 that Go folds for untyped constants).
var labelBoxEm = mulF(1.2, 1.5)

// set reports a present, non-null option (null means unset, like TypeScript's ??).
func set(o Object, k string) bool { v, ok := o[k]; return ok && v != nil }
func opt(options []Object) Object {
	if len(options) == 0 || options[0] == nil {
		return Object{}
	}
	return options[0]
}
func clamp(v, lo, hi float64) float64 { return math.Min(hi, math.Max(lo, v)) }
func round(v float64) float64         { return math.Floor(v + .5) }
func integer(v float64) int {
	if math.IsNaN(v) || math.IsInf(v, 0) || math.Trunc(v) != v || math.Abs(v) > 1<<31 {
		fail("expected integer: %v", v)
	}
	return int(v)
}

// toCount converts a float to an int count, failing like TS's "exceeds 4096 cells" when out of range.
func toCount(v float64) int {
	if math.IsNaN(v) {
		fail("grid exceeds %d cells", MaxMatrixCells)
	}
	limit := float64(MaxMatrixCells + 1) // saturate: anything larger is over the cell limit anyway
	return int(math.Max(-limit, math.Min(limit, v)))
}
func positive(v float64) int {
	i := integer(v)
	if i < 1 {
		fail("expected positive integer")
	}
	return i
}
func finite(v float64) float64 {
	if math.IsNaN(v) || math.IsInf(v, 0) {
		fail("number must be finite")
	}
	return v
}
func join(objects ...Object) Object {
	r := Object{}
	for _, o := range objects {
		for k, v := range o {
			r[k] = v
		}
	}
	return r
}
func clone(v any) any {
	switch x := v.(type) {
	case map[string]any:
		r := Object{}
		for k, v := range x {
			r[k] = clone(v)
		}
		return r
	case []any:
		r := make([]any, len(x))
		for i, v := range x {
			r[i] = clone(v)
		}
		return r
	}
	rv := reflect.ValueOf(v)
	if rv.IsValid() && (rv.Kind() == reflect.Slice || rv.Kind() == reflect.Array) {
		a := array(v)
		r := make([]any, len(a))
		for i, v := range a {
			r[i] = clone(v)
		}
		return r
	}
	return v
}

// batchKey groups numerically equal pixels like TS's JSON.stringify key (-0 spells as 0).
func batchKey(v any) string {
	var normalize func(any) any
	normalize = func(x any) any {
		switch t := x.(type) {
		case float64:
			if t == 0 {
				return 0.
			}
		case map[string]any:
			r := make(map[string]any, len(t))
			for k, v := range t {
				r[k] = normalize(v)
			}
			return r
		}
		return x
	}
	return key(normalize(v))
}
func key(v any) string {
	data, err := json.Marshal(v)
	if err != nil {
		panic(err)
	}
	return string(data)
}
func requireRange(lo, hi float64) {
	finite(lo)
	finite(hi)
	if !(hi > lo) {
		fail("max must exceed min")
	}
}
func fraction(v, lo, hi float64) float64 {
	finite(v)
	if hi <= lo {
		return 0
	}
	return clamp((v-lo)/(hi-lo), 0, 1)
}
func stringsOf(v any) []string {
	a := array(v)
	out := make([]string, len(a))
	for i, v := range a {
		out[i] = v.(string)
	}
	return out
}

var GapModeRatios = map[string]float64{"zero": 0, "near": .08, "default": .16, "far": .28, "extraFar": .42}
