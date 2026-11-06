package glance

import "testing"

func TestSignatureNativeNumericTypes(t *testing.T) {
	cases := []struct {
		value any
		text  string
	}{
		{int64(1000000000000000128), "1000000000000000100"},
		{uint64(1000000000000000128), "1000000000000000100"},
		{float32(0.1), "0.10000000149011612"},
		{int32(1700000000), "1700000000"},
	}
	for _, example := range cases {
		if must(Sign(example.value, "numeric")) != must(Sign(example.text, "numeric")) {
			t.Fatalf("signature of %T differs from JS number text %s", example.value, example.text)
		}
	}
}
