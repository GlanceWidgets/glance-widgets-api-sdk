package glance

import "testing"

func TestOrderedFontAliases(t *testing.T) {
	definition := Object{
		"name":    "ordered-aliases",
		"height":  1,
		"glyphs":  Object{"X": []string{"#"}},
		"aliases": []FontAlias{{Name: "B", Target: "X"}, {Name: "A", Target: "B"}},
	}
	if _, err := MeasureText("AB", definition); err != nil {
		t.Fatalf("backward aliases failed: %v", err)
	}
	definition["aliases"] = []FontAlias{{Name: "A", Target: "B"}, {Name: "B", Target: "X"}}
	if _, err := MeasureText("A", definition); err == nil {
		t.Fatal("forward alias chain was accepted")
	}
	definition["aliases"] = Object{"A": "B", "B": "X"}
	if _, err := MeasureText("A", definition); err == nil {
		t.Fatal("unordered indirect aliases need an explicit ordered definition")
	}
	definition["aliases"] = Object{"A": "X", "B": "X"}
	if _, err := MeasureText("AB", definition); err != nil {
		t.Fatalf("direct unordered aliases failed: %v", err)
	}
}
