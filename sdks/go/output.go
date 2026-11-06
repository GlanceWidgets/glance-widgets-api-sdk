package glance

import (
	"fmt"
	"reflect"
	"regexp"
	"strings"
)

var objectPathPattern = regexp.MustCompile(`^data(?:\.[A-Za-z_][A-Za-z0-9_]*)+$`)

func layerID(id string) string {
	if id == "" {
		fail("layer id must be non-empty")
	}
	if strings.HasPrefix(id, "remote:") {
		return id
	}
	return "remote:" + id
}
func nestAtPath(path string, value any) Object {
	if !objectPathPattern.MatchString(path) {
		fail("expected data.name(.name)* path")
	}
	keys := strings.Split(path, ".")[1:]
	for i := len(keys) - 1; i >= 0; i-- {
		value = Object{keys[i]: value}
	}
	return object(value)
}
func mergeData(target, source Object, strict bool, path string) {
	for key, value := range source {
		existing, exists := target[key]
		a, aok := existing.(map[string]any)
		b, bok := value.(map[string]any)
		if aok && bok {
			mergeData(a, b, strict, path+"."+key)
		} else if exists && strict {
			fail("merge conflict at %s.%s", path, key)
		} else {
			target[key] = clone(value)
		}
	}
}
func merge(parts ...Object) Object {
	strict := false
	if len(parts) > 0 {
		last := parts[len(parts)-1]
		if len(last) == 1 && has(last, "onConflict") {
			strict = last["onConflict"] == "error"
			parts = parts[:len(parts)-1]
		}
	}
	result := Object{"protocol": 1}
	rootKeys := []string{"pixels", "default", "layout", "batch"}
	for _, part := range parts {
		if part == nil {
			fail("merge part must be object")
		}
		if has(part, "content") || has(part, "style") {
			fail("MatrixPart must be placed before merge")
		}
		if has(part, "protocol") && number(part["protocol"]) != 1 {
			fail("unsupported protocol")
		}
		hasRoot := false
		for _, k := range rootKeys {
			hasRoot = hasRoot || has(part, k)
		}
		if hasRoot {
			if !has(part, "pixels") {
				fail("root fields require pixels")
			}
			if has(result, "pixels") && strict {
				fail("merge conflict: root matrix")
			}
			for _, k := range rootKeys {
				delete(result, k)
				if has(part, k) {
					result[k] = clone(part[k])
				}
			}
		}
		if has(part, "data") {
			data, ok := part["data"].(map[string]any)
			if !ok {
				fail("data must be object")
			}
			if !has(result, "data") {
				result["data"] = Object{}
			}
			mergeData(object(result["data"]), data, strict, "data")
		}
		if has(part, "layers") {
			incoming := reflect.ValueOf(part["layers"])
			if !incoming.IsValid() || (incoming.Kind() != reflect.Slice && incoming.Kind() != reflect.Array) ||
				(incoming.Kind() == reflect.Slice && incoming.IsNil()) {
				fail("layers must be a non-null array")
			}
			layers := []any{}
			if has(result, "layers") {
				layers = array(result["layers"])
			}
			for _, value := range array(part["layers"]) {
				layer := object(value)
				index := -1
				for i, l := range layers {
					if object(l)["id"] == layer["id"] {
						index = i
						break
					}
				}
				if index < 0 {
					layers = append(layers, clone(layer))
				} else if strict {
					fail("duplicate layer id")
				} else {
					layers[index] = clone(layer)
				}
			}
			result["layers"] = layers
		}
	}
	return result
}
func layerPath(id string) string {
	name := strings.TrimPrefix(layerID(id), "remote:")
	var out strings.Builder
	out.WriteString("data.layer_")
	for _, c := range name {
		if (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') {
			out.WriteRune(c)
		} else {
			out.WriteString(fmt.Sprintf("_%x_", c))
		}
	}
	return out.String()
}
func combineComponents(parts []any) Object {
	fragments := make([]Object, len(parts))
	for i, input := range parts {
		fragments[i] = object(input)
	}
	merged := merge(fragments...)
	layers := []any{}
	if has(merged, "layers") {
		layers = array(merged["layers"])
	}
	data := any(Object{})
	if has(merged, "data") {
		data = merged["data"]
	}
	return Object{"layers": layers, "data": data}
}
func LayerPath(id string) (string, error) {
	return safe(func() string { return layerPath(id) })
}
func place(part, o Object) Object {
	node := Object{"frame": frameOf(o["frame"]), "content": clone(part["content"]), "style": Object{}}
	for _, k := range []string{"zIndex", "rotation", "transform"} {
		if has(o, k) {
			node[k] = clone(o[k])
		}
	}
	return Object{"layers": []any{Object{"id": layerID(s(o, "id", "")), "type": "matrix", "dataPath": o["path"]}}, "data": nestAtPath(s(o, "path", ""), node)}
}
func emptyRoot() Object {
	return Object{"pixels": []any{[]any{Object{"empty": true}}}, "default": Object{"empty": true}, "layout": Object{"gapMode": "default", "edgeInsetRatio": 0., "cellAspectRatio": 1.}}
}
func toResponse(part Object) Object {
	response := merge(part)
	if !has(response, "pixels") {
		response = merge(emptyRoot(), response)
	}
	validate(response)
	return response
}
func LayerID(id string) (string, error) { return safe(func() string { return layerID(id) }) }
func NestAtPath(path string, value any) (Object, error) {
	return safe(func() Object { return nestAtPath(path, clone(value)) })
}
func Merge(parts ...Object) (Object, error) { return safe(func() Object { return merge(parts...) }) }
func Place(part MatrixPart, options Object) (Composite, error) {
	return safe(func() Object { return place(part, options) })
}
func EmptyRoot() RootMatrix                  { return emptyRoot() }
func ToResponse(part Object) (Object, error) { return safe(func() Object { return toResponse(part) }) }
