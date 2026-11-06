package glance

import "testing"

func TestJavaScriptJSONByteSize(t *testing.T) {
	cases := []struct {
		text  string
		bytes int
	}{
		{"\u2028", 5},
		{"\u2029", 5},
		{`\u2028`, 9},
		{"<>&", 5},
		{"😀", 6},
		{"\n", 4},
	}
	for _, example := range cases {
		if got := jsonByteSize(example.text); got != example.bytes {
			t.Errorf("JSON UTF-8 size of %q = %d, expected %d", example.text, got, example.bytes)
		}
	}
}
