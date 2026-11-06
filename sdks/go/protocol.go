package glance

import (
	"bytes"
	"encoding/json"
	"fmt"
	"math"
	"reflect"
	"regexp"
	"strconv"
	"strings"
	"unicode/utf8"
)

var schemas = loadData("data/schemas.json")
var DataPathPattern = regexp.MustCompile(`^data(?:\.[A-Za-z_][A-Za-z0-9_]*|\[[0-9]+\])*$`)
var pathSegments = regexp.MustCompile(`\.([A-Za-z_][A-Za-z0-9_]*)|\[([0-9]+)\]`)

type GlanceValidationError struct{ Path, Message string }

func (e *GlanceValidationError) Error() string { return e.Path + ": " + e.Message }
func validationFail(path, message string)      { panic(&GlanceValidationError{path, message}) }
func checkObject(value any, schema Object, path string) Object {
	o, ok := value.(map[string]any)
	if !ok {
		validationFail(path, "expected object")
	}
	for k, rule := range schema {
		if v, exists := o[k]; exists && v != nil {
			checkRule(v, rule, path+"."+k)
		}
	}
	return o
}
func checkRule(value, rule any, path string) {
	if name, ok := rule.(string); ok {
		valid := false
		switch name {
		case "number", "integer":
			if reflect.TypeOf(value) != nil {
				kind := reflect.TypeOf(value).Kind()
				valid = (kind >= reflect.Int && kind <= reflect.Float64) && kind != reflect.Uintptr
			}
			if valid && name == "integer" {
				v := number(value)
				valid = math.Trunc(v) == v
			}
		case "textValue":
			_, valid = value.(string)
			if !valid && value != nil {
				k := reflect.TypeOf(value).Kind()
				valid = k >= reflect.Int && k <= reflect.Float64 && k != reflect.Uintptr
			}
		case "string":
			_, valid = value.(string)
		case "boolean":
			_, valid = value.(bool)
		case "color":
			valid = IsAppColor(value)
		}
		if !valid {
			validationFail(path, "expected "+name)
		}
		return
	}
	r := object(rule)
	if enum, ok := r["enum"]; ok {
		valid := false
		v, isString := value.(string)
		if isString {
			for _, candidate := range array(enum) {
				if v == candidate {
					valid = true
				}
			}
		}
		if !valid {
			validationFail(path, "invalid enum")
		}
	} else if item, ok := r["arrayOf"]; ok {
		a, isArray := value.([]any)
		if !isArray {
			validationFail(path, "expected array")
		}
		if has(r, "length") && len(a) != integer(n(r, "length", 0)) {
			validationFail(path, "invalid array length")
		}
		for i, v := range a {
			checkRule(v, item, fmt.Sprintf("%s[%d]", path, i))
		}
	} else if nested, ok := r["object"]; ok {
		checkObject(value, object(nested), path)
	} else {
		o, ok := value.(map[string]any)
		if !ok {
			validationFail(path, "expected record")
		}
		for k, v := range o {
			checkRule(v, r["record"], path+"."+k)
		}
	}
}
func checkJSON(value any, path string) {
	switch v := value.(type) {
	case nil, string, bool:
		return
	case json.Number:
		f, err := v.Float64()
		if err != nil {
			validationFail(path, "invalid JSON number")
		}
		checkJSON(f, path)
	case map[string]any:
		for k, item := range v {
			checkJSON(item, path+"."+k)
		}
	case []any:
		for i, item := range v {
			checkJSON(item, fmt.Sprintf("%s[%d]", path, i))
		}
	case float64:
		if math.IsNaN(v) || math.IsInf(v, 0) || math.Abs(v) >= MaxSafeNativeInteger {
			validationFail(path, "number unsafe for native renderer")
		}
	default:
		rv := reflect.ValueOf(value)
		if rv.Kind() >= reflect.Int && rv.Kind() <= reflect.Float64 {
			checkJSON(number(value), path)
		} else if rv.Kind() == reflect.Slice || rv.Kind() == reflect.Array {
			checkJSON(array(value), path)
		} else {
			validationFail(path, "value is not JSON")
		}
	}
}
func checkMatrix(value Object, path string) {
	pixels, ok := value["pixels"].([]any)
	if !ok || len(pixels) == 0 {
		validationFail(path+".pixels", "expected non-empty 2D array")
	}
	first, ok := pixels[0].([]any)
	if !ok || len(first) == 0 {
		validationFail(path+".pixels[0]", "expected nonempty row")
	}
	rows, columns := len(pixels), len(first)
	if rows*columns > 4096 {
		validationFail(path+".pixels", "matrix exceeds 4096 cells")
	}
	for y, row := range pixels {
		a, ok := row.([]any)
		p := fmt.Sprintf("%s.pixels[%d]", path, y)
		if !ok || len(a) != columns {
			validationFail(p, "inconsistent row width")
		}
		for x, pixel := range a {
			if pixel != nil {
				checkObject(pixel, object(schemas["pixel"]), fmt.Sprintf("%s[%d]", p, x))
			}
		}
	}
	for _, k := range []string{"default", "layout"} {
		if v, ok := value[k]; ok {
			schema := "pixel"
			if k == "layout" {
				schema = "layout"
			}
			checkObject(v, object(schemas[schema]), path+"."+k)
		}
	}
	if has(value, "batch") {
		entries, ok := value["batch"].([]any)
		if !ok {
			validationFail(path+".batch", "expected array")
		}
		for i, e := range entries {
			p := fmt.Sprintf("%s.batch[%d]", path, i)
			entry := checkObject(e, Object{"pixel": Object{"object": schemas["pixel"]}}, p)
			coordinates, ok := entry["coordinates"].([]any)
			if !ok {
				validationFail(p+".coordinates", "expected array")
			}
			for j, pair := range coordinates {
				pp := fmt.Sprintf("%s.coordinates[%d]", p, j)
				checkRule(pair, Object{"arrayOf": "integer", "length": 2.}, pp)
				a := pair.([]any)
				r, c := number(a[0]), number(a[1])
				if r < 0 || c < 0 || r >= float64(rows) || c >= float64(columns) {
					validationFail(pp, "coordinates outside matrix")
				}
			}
		}
	}
}
func resolveDataPath(data any, path string) any {
	if !DataPathPattern.MatchString(path) {
		return nil
	}
	current := data
	for _, match := range pathSegments.FindAllStringSubmatch(strings.TrimPrefix(path, "data"), -1) {
		if match[1] != "" {
			o, ok := current.(map[string]any)
			if !ok {
				return nil
			}
			current = o[match[1]]
		} else {
			rv := reflect.ValueOf(current)
			if !rv.IsValid() || (rv.Kind() != reflect.Slice && rv.Kind() != reflect.Array) {
				return nil
			}
			a := array(current)
			index, err := strconv.Atoi(match[2])
			if err != nil || index < 0 || index >= len(a) {
				return nil
			}
			current = a[index]
		}
		if current == nil {
			return nil
		}
	}
	return current
}

// Count the UTF-8 bytes JSON.stringify would emit. Go's encoder escapes U+2028
// and U+2029 even with HTML escaping disabled; escaped literal backslashes must
// remain ordinary string content, not be replaced by a post-encoding search.
func jsonStringByteSize(text string) int {
	size := 2 // opening and closing quotes
	for _, character := range text {
		switch character {
		case '"', '\\', '\b', '\f', '\n', '\r', '\t':
			size += 2
		default:
			if character < 0x20 {
				size += 6
			} else {
				size += utf8.RuneLen(character)
			}
		}
	}
	return size
}

func jsonByteSize(value any) int {
	switch value := value.(type) {
	case nil:
		return 4
	case bool:
		if value {
			return 4
		}
		return 5
	case float64:
		return len(numberString(value))
	case string:
		return jsonStringByteSize(value)
	case []any:
		size := 2
		for index, item := range value {
			if index > 0 {
				size++
			}
			size += jsonByteSize(item)
		}
		return size
	case map[string]any:
		size := 2
		first := true
		for key, item := range value {
			if !first {
				size++
			}
			first = false
			size += jsonStringByteSize(key) + 1 + jsonByteSize(item)
		}
		return size
	default:
		fail("unexpected JSON type while counting bytes: %T", value)
		return 0
	}
}

func validate(response Object) {
	if response == nil {
		validationFail("$", "response must be object")
	}
	checkJSON(response, "$")
	var buffer bytes.Buffer
	encoder := json.NewEncoder(&buffer)
	encoder.SetEscapeHTML(false)
	if err := encoder.Encode(response); err != nil {
		validationFail("$", err.Error())
	}
	encoded := bytes.TrimSuffix(buffer.Bytes(), []byte{'\n'})
	var normalized Object
	if err := json.Unmarshal(encoded, &normalized); err != nil {
		panic(err)
	}
	response = normalized
	if jsonByteSize(response) > MaxResponseBytes {
		validationFail("$", "response exceeds 512 KB")
	}
	if has(response, "protocol") {
		v, ok := response["protocol"].(float64)
		if !ok || v != 1 {
			validationFail("$.protocol", "must be number 1")
		}
	}
	checkMatrix(response, "$")
	data := response["data"]
	if data == nil {
		data = Object{}
	}
	if _, ok := data.(map[string]any); !ok {
		validationFail("$.data", "expected object")
	}
	if !has(response, "layers") {
		return
	}
	layers, ok := response["layers"].([]any)
	if !ok {
		validationFail("$.layers", "expected array")
	}
	seen := map[string]bool{}
	for i, l := range layers {
		path := fmt.Sprintf("$.layers[%d]", i)
		layer := checkStrictObject(l, Object{"id": "string", "type": "string", "dataPath": "string"}, path)
		id, ok := layer["id"].(string)
		if !ok || !strings.HasPrefix(id, "remote:") || len(id) <= 7 || seen[id] {
			validationFail(path+".id", "must be unique remote: id")
		}
		seen[id] = true
		kind, ok := layer["type"].(string)
		schema, exists := object(schemas["layers"])[kind]
		if !ok || !exists {
			validationFail(path+".type", "unknown layer type")
		}
		binding, ok := layer["dataPath"].(string)
		if !ok || !DataPathPattern.MatchString(binding) {
			validationFail(path+".dataPath", "invalid dataPath")
		}
		checkLayerNode(resolveDataPath(data, binding), object(schema), kind, binding, true)
	}
}
func Validate(response Object) error {
	_, err := safe(func() bool { validate(response); return true })
	return err
}
func ResolveDataPath(data any, path string) any { return resolveDataPath(data, path) }

func checkStrictObject(value any, schema Object, path string) Object {
	o := checkObject(value, schema, path)
	for k := range o {
		if _, ok := schema[k]; !ok {
			validationFail(path+"."+k, "unsupported field")
		}
	}
	return o
}
func checkLayerNode(value any, schema Object, kind, path string, requiresFrame bool) {
	node := checkObject(value, Object{}, path)
	keys := Object{"frame": nil, "zIndex": nil, "rotation": nil, "transform": nil, "content": nil, "style": nil}
	for k := range node {
		if _, ok := keys[k]; !ok {
			validationFail(path+"."+k, "unsupported field")
		}
	}
	frame, ok := node["frame"].(map[string]any)
	if (node["frame"] != nil || requiresFrame) && (!ok || frame["width"] == nil || frame["height"] == nil) {
		validationFail(path+".frame", "remote frame needs width and height")
	}
	if node["frame"] != nil {
		checkStrictObject(frame, object(schemas["frame"]), path+".frame")
	}
	for _, k := range []string{"zIndex", "rotation"} {
		if node[k] != nil {
			rule := "number"
			if k == "zIndex" {
				rule = "integer"
			}
			checkRule(node[k], rule, path+"."+k)
		}
	}
	if node["transform"] != nil {
		checkStrictObject(node["transform"], object(schemas["transform"]), path+".transform")
	}
	content := Object{}
	if node["content"] != nil {
		content = checkObject(node["content"], Object{}, path+".content")
	}
	if kind == "matrix" {

		allowed := Object{"pixels": nil, "default": nil, "batch": nil, "layout": nil}
		for k := range content {
			if _, ok := allowed[k]; !ok {
				validationFail(path+".content."+k, "unsupported field")
			}
		}
		filtered := Object{}
		for k, v := range content {
			if v != nil {
				filtered[k] = v
			}
		}
		content = filtered
		if content["pixels"] != nil || requiresFrame {
			checkMatrix(content, path+".content")
		} else if content["batch"] != nil {
			validationFail(path+".content.batch", "batch requires pixels")
		} else if content["default"] != nil {
			checkMatrix(Object{"pixels": []any{[]any{nil}}, "default": content["default"]}, path+".content")
		}
		if content["layout"] != nil {
			checkStrictObject(content["layout"], object(schemas["layout"]), path+".content.layout")
		}
	} else {
		checkStrictObject(content, object(schema["content"]), path+".content")
	}
	if node["style"] != nil {
		checkStrictObject(node["style"], object(schema["style"]), path+".style")
		if colors := object(node["style"])["gradientColors"]; colors != nil && len(array(colors)) == 0 {
			validationFail(path+".style.gradientColors", "must contain at least one color")
		}
	}
	if kind == "image" || kind == "icon" {
		glyph := content["icon"] != nil
		if (glyph && content["url"] != nil) || (!glyph && (content["iconCodePoint"] != nil || content["iconFont"] != nil)) {
			validationFail(path+".content.icon", "choose one artwork source")
		}
	}
}

// ValidateLayerNode validates an editor partial node by default. Optional settings accept path and requiresFrame.
func ValidateLayerNode(node Object, kind string, settings ...Object) error {
	_, err := safe(func() bool {
		o := opt(settings)
		path := s(o, "path", "$")
		checkJSON(node, path)
		schema, ok := object(schemas["layers"])[kind]
		if !ok {
			validationFail(path, "unknown layer type")
		}
		checkLayerNode(node, object(schema), kind, path, b(o, "requiresFrame", false))
		return true
	})
	return err
}
