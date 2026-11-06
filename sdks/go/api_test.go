package glance

import (
	"encoding/json"
	"errors"
	"math"
	"reflect"
	"strings"
	"sync"
	"testing"
)

func TestNativeNumericInputs(t *testing.T) {
	for _, value := range []any{int8(7), int16(7), uint8(7), uint16(7), uint32(7), json.Number("7")} {
		if _, err := Stat(value, Object{"canvas": Canvas{338, 158}}); err != nil {
			t.Fatal(err)
		}
	}
	if _, err := Heatmap([][]uint8{{0, 255}}); err != nil {
		t.Fatal(err)
	}
	data := Object{"items": []Object{{"value": 3}}}
	if got := ResolveDataPath(data, "data.items[0].value"); got != 3 {
		t.Fatalf("typed slice path: %v", got)
	}
	decoder := json.NewDecoder(strings.NewReader(`{"pixels":[[null]],"data":{"n":1}}`))
	decoder.UseNumber()
	var response Object
	if err := decoder.Decode(&response); err != nil {
		t.Fatal(err)
	}
	if err := Validate(response); err != nil {
		t.Fatal(err)
	}
}

func TestExponentNumberFormatting(t *testing.T) {
	for value, want := range map[float64]string{1e21: "1e+21", -1e21: "-1e+21", 1e-7: "1e-7", 1e20: "100000000000000000000", 1e-6: "0.000001"} {
		if got := numberString(value); got != want {
			t.Fatalf("%v: %s != %s", value, got, want)
		}
		if must(Sign(value, "x")) != must(Sign(want, "x")) {
			t.Fatalf("signature number formatting mismatch for %v", value)
		}
	}
	if got := formatNumber(1e21, 2); got != "1e+21" {
		t.Fatal(got)
	}
}

func TestPublicResponse(t *testing.T) {
	canvas := Canvas{338, 158}
	box := must(CanvasBox(canvas).Sub(.1, .75, .8, .15))
	frame := must(box.ToFrame())
	root := must(PixelTextRoot("12:30", Object{"canvas": canvas, "theme": "led"}))
	bar := must(PixelProgressBar(65, Object{"canvas": canvas, "frame": frame}))
	placed := must(Place(bar, Object{"id": "progress", "frame": frame, "path": "data.progress"}))
	response := must(ToResponse(must(Merge(root, placed))))
	if err := Validate(response); err != nil {
		t.Fatal(err)
	}
}
func TestPublicValidation(t *testing.T) {
	cases := []Object{
		{"protocol": true, "pixels": []any{[]any{nil}}},
		{"pixels": []any{[]any{Object{"opacity": true}}}},
		{"pixels": []any{[]any{nil}}, "data": Object{"value": math.NaN()}},
		{"pixels": []any{[]any{nil}}, "data": Object{"value": 9.2e18}},
		{"pixels": []any{[]any{nil}}, "batch": []any{Object{"pixel": Object{}, "coordinates": []any{[]any{1, 0}}}}},
	}
	for _, response := range cases {
		var validation *GlanceValidationError
		err := Validate(response)
		if !errors.As(err, &validation) {
			t.Fatalf("expected path-aware validation error: %v", err)
		}
	}
	response := must(ToResponse(Object{}))
	response["pixels"] = []any{[]any{Object{"opacity": true}}}
	var validation *GlanceValidationError
	if err := Validate(response); !errors.As(err, &validation) || validation.Path != "$.pixels[0][0].opacity" {
		t.Fatalf("wrong error path: %v", err)
	}
	part := must(NewMatrix(2, 2)).ToPart()
	response = must(ToResponse(must(Place(part, Object{"id": "grid", "frame": Object{"width": 1, "height": 1}, "path": "data.grid"}))))
	delete(object(response["data"]), "grid")
	if Validate(response) == nil {
		t.Fatal("unresolved matrix binding accepted")
	}
}
func TestMergePurity(t *testing.T) {
	a := Object{"data": Object{"cpu": Object{"value": 1}}}
	b := Object{"data": Object{"cpu": Object{"unit": "%"}}}
	c := Object{"data": Object{"cpu": Object{"value": 2}}}
	before := key([]any{a, b, c})
	left := must(Merge(must(Merge(a, b)), c))
	right := must(Merge(a, must(Merge(b, c))))
	if !reflect.DeepEqual(left, right) {
		t.Fatal("merge not associative")
	}
	object(object(left["data"])["cpu"])["unit"] = "changed"
	if key([]any{a, b, c}) != before {
		t.Fatal("merge mutated input")
	}
}
func TestIndependentMatrix(t *testing.T) {
	m := must(NewMatrix(2, 1))
	must(m.Set(0, 0, "#f00"))
	copy := m.FlipX()
	must(copy.Set(0, 0, "#0f0"))
	if s(object(must(m.Get(0, 0))), "color", "") != "#FF0000" {
		t.Fatal("transform shared cells")
	}
	visible := must(NewMatrix(1, 1, Object{"default": Object{}}))
	if !reflect.DeepEqual(visible.ToRoot()["default"], DefaultPixelBase) {
		t.Fatal("empty default object treated as empty cell")
	}
}
func TestExactBinaryRounding(t *testing.T) {
	for _, c := range []struct {
		v    float64
		d    int
		want string
	}{{2.5, 0, "3"}, {-2.5, 0, "-3"}, {.125, 2, "0.13"}, {2.55, 1, "2.5"}, {1.005, 2, "1.00"}, {-.01, 1, "0.0"}} {
		if got := formatNumber(c.v, c.d); got != c.want {
			t.Fatalf("toFixed(%v,%d)=%s !=%s", c.v, c.d, got, c.want)
		}
	}
}
func TestUnicodeAndSignatures(t *testing.T) {
	definition := Object{"name": "go-unicode", "height": 1, "glyphs": Object{"😀": []string{"#"}}}
	font := must(DefineFont(definition))
	if !font.HasGlyph("😀") || n(must(MeasureText("😀😀", font)), "width", 0) != 3 {
		t.Fatal("font did not count Unicode code points")
	}
	secret := "密钥🔑"
	ts := "1700000000000"
	request := Object{"timestamp": ts, "signature": must(Sign(ts, secret))}
	if !VerifySignature(request, secret, Object{"now": 1700000000000.}) || VerifySignature(request, "wrong", Object{"now": 1700000000000.}) {
		t.Fatal("signature verification mismatch")
	}
	if must(Sign(1700000000000., "this is secret")) != "sSFWELbV2YwjdDQhWZwTcWlX5BWUx5J6TPpsZmuPii0=" {
		t.Fatal("numeric timestamp formatting mismatch")
	}
}
func TestConcurrentFontRegistration(t *testing.T) {
	var wg sync.WaitGroup
	for i := 0; i < 8; i++ {
		i := i
		wg.Add(1)
		go func() {
			defer wg.Done()
			_, err := DefineFont(Object{"name": "concurrent-" + formatNumber(float64(i), 0), "height": 1, "glyphs": Object{"X": []string{"#"}}})
			if err != nil {
				t.Error(err)
			}
			for j := 0; j < 20; j++ {
				if _, err := MeasureText("OK", "3x5"); err != nil {
					t.Error(err)
				}
			}
		}()
	}
	wg.Wait()
}
func TestBundledData(t *testing.T) {
	if len(IconNames()) != 5287 {
		t.Fatal("missing embedded icons")
	}
	if !HasIcon("ph_star") {
		t.Fatal("missing icon")
	}
	must(ToResponse(must(PixelTextRoot("OK", Object{"canvas": Canvas{338, 158}}))))
	must(ToResponse(must(PixelBarChart([]float64{1, -2, 4}, Object{"canvas": Canvas{338, 158}}))))
	for _, composite := range []Object{must(ProgressBar(50, Object{"canvas": Canvas{338, 158}})), must(RingProgress(50, Object{"canvas": Canvas{338, 158}})), must(BarChart([]float64{1, -2, 4}, Object{"canvas": Canvas{338, 158}})), must(Battery(50, Object{"canvas": Canvas{338, 158}})), must(Signal(50, Object{"canvas": Canvas{338, 158}}))} {
		must(ToResponse(composite))
	}
	for _, part := range []Object{must(PixelProgressBar(50)), must(PixelRingProgress(30)), must(StatusStrip([]any{"ok", nil, "down"})), must(Heatmap([][]float64{{1, 2}, {3, 4}}))} {
		must(ToResponse(must(Place(part, Object{"id": "part", "frame": Object{"width": 1, "height": 1}, "path": "data.part"}))))
	}
}

func TestLayerNodes(t *testing.T) {
	part := must(Text(Object{"id": "title:😀", "frame": Object{"width": 1, "height": 1}, "text": "hello", "transform": Object{"translateX": .1}}))
	if object(array(part["layers"])[0])["dataPath"] != "data.layer_title_3a__1f600_" {
		t.Fatal(part)
	}
	response := must(ToResponse(part))
	declaration := object(array(response["layers"])[0])
	if len(declaration) != 3 {
		t.Fatal(declaration)
	}
	node := object(ResolveDataPath(response["data"], declaration["dataPath"].(string)))
	object(node["style"])["textOpacity"] = nil
	if err := Validate(response); err != nil {
		t.Fatal(err)
	}
	if err := ValidateLayerNode(Object{"content": Object{"text": 42}, "style": Object{"textOpacity": 0}}, "text"); err != nil {
		t.Fatal(err)
	}
	if err := ValidateLayerNode(Object{"content": Object{"default": Object{"color": "#FFFFFF"}}}, "matrix"); err != nil {
		t.Fatal(err)
	}
	for _, bad := range []Object{{"frame": Object{"width": 1}}, {"content": Object{"sourceMode": "api"}}, {"style": Object{"unknown": false}}} {
		if ValidateLayerNode(bad, "text") == nil {
			t.Fatal("invalid node accepted", bad)
		}
	}
	if ValidateLayerNode(Object{"content": Object{"batch": []any{}}}, "matrix") == nil {
		t.Fatal("batch without pixels accepted")
	}
	duplicate := clone(response).(map[string]any)
	duplicate["layers"] = append(array(duplicate["layers"]), declaration)
	if Validate(duplicate) == nil {
		t.Fatal("duplicate id accepted")
	}
}

func TestMergeReplacesAndCopies(t *testing.T) {
	frame := Object{"width": 1, "height": 1}
	first := must(Text(Object{"id": "label", "frame": frame, "text": "first"}))
	changed := must(Text(Object{"id": "label", "frame": frame, "text": "changed"}))
	last := must(Text(Object{"id": "last", "frame": frame}))
	result := must(Merge(first, last, changed))
	declarations := array(result["layers"])
	if len(declarations) != 2 || object(declarations[1])["id"] != "remote:last" {
		t.Fatal(result)
	}
	node := object(ResolveDataPath(result["data"], object(array(first["layers"])[0])["dataPath"].(string)))
	if object(node["content"])["text"] != "changed" {
		t.Fatal(node)
	}
	object(node["content"])["text"] = "mutated"
	object(declarations[0])["id"] = "mutated"
	if object(object(ResolveDataPath(changed["data"], "data.layer_label"))["content"])["text"] != "changed" || object(array(first["layers"])[0])["id"] != "remote:label" {
		t.Fatal("composition shares input")
	}
	if ValidateLayerNode(Object{"style": Object{"gradientColors": []any{}}}, "shape") == nil {
		t.Fatal("empty colors accepted")
	}
}
