package glance

import (
	"bytes"
	"encoding/json"
	"fmt"
)

// Decode input font alias objects in JSON key order. The SDK receives its native
// ordered alias representation; no expected fixture values are used here.
func decodeGoldenValue(decoder *json.Decoder) (any, []string, error) {
	token, err := decoder.Token()
	if err != nil {
		return nil, nil, err
	}
	delimiter, compound := token.(json.Delim)
	if !compound {
		return token, nil, nil
	}
	if delimiter == '[' {
		values := []any{}
		for decoder.More() {
			value, _, err := decodeGoldenValue(decoder)
			if err != nil {
				return nil, nil, err
			}
			values = append(values, value)
		}
		_, err = decoder.Token()
		return values, nil, err
	}
	if delimiter != '{' {
		return nil, nil, fmt.Errorf("unexpected JSON delimiter: %s", string(delimiter))
	}
	fields := Object{}
	keys := []string{}
	aliasKeys := []string{}
	for decoder.More() {
		key, err := decoder.Token()
		if err != nil {
			return nil, nil, err
		}
		name := key.(string)
		value, childKeys, err := decodeGoldenValue(decoder)
		if err != nil {
			return nil, nil, err
		}
		fields[name] = value
		keys = append(keys, name)
		if name == "aliases" {
			aliasKeys = childKeys
		}
	}
	if _, err = decoder.Token(); err != nil {
		return nil, nil, err
	}
	if has(fields, "glyphs") && has(fields, "height") && has(fields, "aliases") {
		definitions := object(fields["aliases"])
		aliases := []FontAlias{}
		for _, name := range aliasKeys {
			aliases = append(aliases, FontAlias{Name: name, Target: definitions[name].(string)})
		}
		fields["aliases"] = aliases
	}
	return fields, keys, nil
}

func decodeGoldenCases(data []byte) ([]Object, error) {
	value, _, err := decodeGoldenValue(json.NewDecoder(bytes.NewReader(data)))
	if err != nil {
		return nil, err
	}
	cases := []Object{}
	for _, entry := range array(value) {
		cases = append(cases, object(entry))
	}
	return cases, nil
}
