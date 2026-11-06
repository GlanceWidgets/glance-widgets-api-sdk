package glance

import (
	"embed"
	"encoding/json"
	"math"
	"sort"
	"strings"
	"sync"
	"unicode/utf8"
)

//go:generate python3 scripts/generate_data.py
//go:embed data/*.json data/fonts/*.json
var bundledData embed.FS

func loadData(path string) Object {
	data, err := bundledData.ReadFile(path)
	if err != nil {
		panic(err)
	}
	var o Object
	if err = json.Unmarshal(data, &o); err != nil {
		panic(err)
	}
	return o
}

const DefaultFontName = "3x5"
const DefaultLetterSpacing = 1
const DefaultLineSpacing = 1
const AnchorBoxWidth = 1

type Glyph struct {
	Rows                      []string
	Width, Advance, InkBottom int
}

// FontAlias defines a glyph alias. Use []FontAlias when aliases refer to earlier aliases.
// A plain Object remains suitable for aliases pointing directly at original glyphs.
type FontAlias struct {
	Name   string
	Target string
}

type PixelFont struct {
	Name                                 string
	Height, Descent, Spacing, LineHeight int
	glyphs                               map[rune]Glyph
	monospaceAdvance                     int
	fallback                             rune
	hasFallback                          bool
}

var fontRegistry = map[string]*PixelFont{}
var fontRegistryMu sync.RWMutex
var Font3x5, Font5x7 *PixelFont

func newPixelFont(d Object) *PixelFont {
	font := &PixelFont{Name: s(d, "name", ""), Height: integer(n(d, "height", 0)), Descent: integer(n(d, "descent", 0)), Spacing: integer(n(d, "spacing", 1)), glyphs: map[rune]Glyph{}}
	if font.Height < 1 || font.Descent < 0 {
		fail("font requires positive height and non-negative descent")
	}
	font.LineHeight = font.Height + font.Descent
	fixed := 0
	if has(d, "advance") {
		if d["advance"] == nil {
			fail("font advance must be an integer")
		}
		fixed = integer(n(d, "advance", 0))
	}
	for character, definition := range object(d["glyphs"]) {
		chars := []rune(character)
		if len(chars) != 1 {
			fail("glyph key must be one Unicode code point")
		}
		var rows []string
		advance := 0
		hasAdvance, advanceSet := false, false
		if o, ok := definition.(map[string]any); ok {
			rows = stringsOf(o["rows"])
			hasAdvance, advanceSet = has(o, "advance"), set(o, "advance")
			advance = integer(n(o, "advance", 0))
		} else {
			rows = stringsOf(definition)
		}
		width := 0
		if len(rows) > 0 {
			width = utf8.RuneCountInString(rows[0])
		}
		if len(rows) > font.LineHeight {
			fail("too many glyph rows")
		}
		for _, row := range rows {
			if utf8.RuneCountInString(row) != width {
				fail("glyph row widths must match")
			}
			for _, c := range row {
				if c != '#' && c != '.' {
					fail("glyph rows contain invalid character")
				}
			}
		}
		if len(rows) == 0 && !has(d, "advance") && !hasAdvance {
			fail("glyph without rows needs an advance")
		}
		if has(d, "advance") {
			advance = fixed
		} else if !advanceSet {
			advance = width
		}
		if advance < 0 || (has(d, "advance") && width > fixed) {
			fail("invalid glyph advance")
		}
		for len(rows) < font.Height {
			rows = append(rows, strings.Repeat(".", width))
		}
		ink := 0
		for i, row := range rows {
			if strings.Contains(row, "#") {
				ink = i + 1
			}
		}
		font.glyphs[chars[0]] = Glyph{rows, width, advance, ink}
		font.monospaceAdvance = max(font.monospaceAdvance, max(width, advance))
	}
	font.addAliases(d["aliases"])
	if has(d, "fallback") {
		chars := []rune(s(d, "fallback", ""))
		if len(chars) != 1 {
			fail("fallback must be one code point")
		}
		font.fallback, font.hasFallback = chars[0], true
		if _, ok := font.glyphs[font.fallback]; !ok {
			fail("fallback has no glyph")
		}
	}
	return font
}
func (font *PixelFont) addAliases(value any) {
	aliases, ordered := value.([]FontAlias)
	if !ordered {
		definitions := object(value)
		keys := make([]string, 0, len(definitions))
		for name, target := range definitions {
			keys = append(keys, name)
			characters := []rune(target.(string))
			if len(characters) != 1 {
				fail("alias target must be one character")
			}
			if _, exists := font.glyphs[characters[0]]; !exists {
				fail("unordered aliases must target original glyphs; use []FontAlias for ordered chains")
			}
		}
		sort.Strings(keys)
		for _, name := range keys {
			aliases = append(aliases, FontAlias{Name: name, Target: definitions[name].(string)})
		}
	}
	for _, alias := range aliases {
		name, target := []rune(alias.Name), []rune(alias.Target)
		if len(name) != 1 || len(target) != 1 {
			fail("font alias and target must each be one Unicode code point")
		}
		glyph, exists := font.glyphs[target[0]]
		if !exists {
			fail("font alias refers to a missing glyph")
		}
		if _, exists := font.glyphs[name[0]]; !exists {
			font.glyphs[name[0]] = glyph
		}
	}
}

func defineFont(d Object) *PixelFont {
	fontRegistryMu.Lock()
	defer fontRegistryMu.Unlock()
	name := s(d, "name", "")
	if _, ok := fontRegistry[name]; ok {
		fail("font already defined")
	}
	f := newPixelFont(d)
	fontRegistry[name] = f
	return f
}
func getFont(name string) *PixelFont {
	fontRegistryMu.RLock()
	defer fontRegistryMu.RUnlock()
	if f, ok := fontRegistry[name]; ok {
		return f
	}
	fail("unknown font %q", name)
	return nil
}
func resolveFont(value any) *PixelFont {
	if value == nil {
		return getFont("3x5")
	}
	switch v := value.(type) {
	case string:
		return getFont(v)
	case *PixelFont:
		return v
	default:
		return newPixelFont(object(v))
	}
}
func (f *PixelFont) glyphFor(c rune) Glyph {
	if g, ok := f.glyphs[c]; ok {
		return g
	}
	if f.hasFallback {
		return f.glyphs[f.fallback]
	}
	fail("font %s has no glyph for %U", f.Name, c)
	return Glyph{}
}
func (f *PixelFont) HasGlyph(character string) bool {
	r := []rune(character)
	if len(r) != 1 {
		return false
	}
	_, ok := f.glyphs[r[0]]
	return ok
}
func (f *PixelFont) GlyphFor(character string) (Glyph, error) {
	return safe(func() Glyph {
		r := []rune(character)
		if len(r) != 1 {
			fail("expected one Unicode character")
		}
		return f.glyphFor(r[0])
	})
}
func DefineFont(definition Object) (*PixelFont, error) {
	return safe(func() *PixelFont { return defineFont(definition) })
}
func GetFont(name string) (*PixelFont, error) {
	return safe(func() *PixelFont { return getFont(name) })
}
func ResolveFont(font any) (*PixelFont, error) {
	return safe(func() *PixelFont { return resolveFont(font) })
}

type placedGlyph struct {
	Glyph Glyph
	X, Y  int
}
type textLayout struct {
	Width, Height int
	Lines         []string
	Glyphs        []placedGlyph
}

func layoutText(content string, o Object) textLayout {
	font := resolveFont(o["font"])
	monospace := b(o, "monospace", false)
	advance := func(g Glyph) int {
		if monospace {
			return font.monospaceAdvance
		}
		return g.Advance
	}
	spacing, lineSpacing := integer(n(o, "letterSpacing", float64(font.Spacing))), integer(n(o, "lineSpacing", 1))
	boxWidth := integer(n(o, "boxWidth", n(o, "maxWidth", 1)))
	align := s(o, "align", "left")
	if align != "left" && align != "center" && align != "right" {
		fail("invalid text alignment")
	}
	maxWidth := 0
	if set(o, "maxWidth") {
		maxWidth = positive(n(o, "maxWidth", 0))
	}
	lineWidth := func(line []rune) int {
		if len(line) == 0 {
			return 0
		}
		width := spacing * (len(line) - 1)
		for _, c := range line {
			width += advance(font.glyphFor(c))
		}
		return width
	}
	lines := [][]rune{}
	for _, paragraph := range strings.Split(content, "\n") {
		if maxWidth == 0 {
			lines = append(lines, []rune(paragraph))
			continue
		}
		var current []rune
		isCurrent := false
		for _, word := range strings.Split(paragraph, " ") {
			rest := []rune(word)
			candidate := append([]rune{}, rest...)
			if isCurrent {
				candidate = append(append(append([]rune{}, current...), ' '), rest...)
			}
			if lineWidth(candidate) <= maxWidth {
				current = candidate
				isCurrent = true
				continue
			}
			if isCurrent {
				lines = append(lines, current)
			}
			for len(rest) > 1 && lineWidth(rest) > maxWidth {
				length := 1
				for length < len(rest) && lineWidth(rest[:length+1]) <= maxWidth {
					length++
				}
				lines = append(lines, rest[:length])
				rest = rest[length:]
			}
			current = rest
			isCurrent = true
		}
		lines = append(lines, current)
	}
	out := textLayout{Lines: []string{}, Glyphs: []placedGlyph{}}
	pitch := font.LineHeight + lineSpacing
	for i, line := range lines {
		width := lineWidth(line)
		out.Width = maxInt(out.Width, width)
		pen := 0
		if align == "right" {
			pen = boxWidth - width
		} else if align == "center" {
			pen = int(math.Floor(float64(boxWidth-width) / 2))
		}
		for _, c := range line {
			glyph := font.glyphFor(c)
			offset := 0
			if monospace {
				offset = (font.monospaceAdvance - glyph.Width) / 2
			}
			out.Glyphs = append(out.Glyphs, placedGlyph{glyph, pen + offset, i * pitch})
			pen += advance(glyph) + spacing
		}
		out.Lines = append(out.Lines, string(line))
	}
	height := font.Height
	for _, c := range lines[len(lines)-1] {
		height = maxInt(height, font.glyphFor(c).InkBottom)
	}
	out.Height = (len(lines)-1)*pitch + height
	return out
}
func metricsObject(layout textLayout) Object {
	return Object{"width": layout.Width, "height": layout.Height, "lines": layout.Lines}
}
func measureText(content string, font any, o Object) Object {
	return metricsObject(layoutText(content, join(o, Object{"font": font})))
}
func LayoutText(content string, options ...Object) (Object, error) {
	return safe(func() Object {
		l := layoutText(content, opt(options))
		out := metricsObject(l)
		glyphs := []any{}
		for _, p := range l.Glyphs {
			g := p.Glyph
			glyphs = append(glyphs, Object{"x": p.X, "y": p.Y, "glyph": Object{"rows": g.Rows, "width": g.Width, "advance": g.Advance, "inkBottom": g.InkBottom}})
		}
		out["glyphs"] = glyphs
		return out
	})
}
func MeasureText(content string, font any, options ...Object) (Object, error) {
	return safe(func() Object { return measureText(content, font, opt(options)) })
}
func (m *Matrix) text(content string, x, y int, o Object) *Matrix {
	style := o["style"]
	if !has(o, "style") {
		style = m.Base
	}
	cell := resolvePixel(style, m.Base)
	for _, placed := range layoutText(content, o).Glyphs {
		for r, row := range placed.Glyph.Rows {
			for c, ch := range row {
				if ch == '#' {
					m.plot(x+placed.X+c, y+placed.Y+r, cell)
				}
			}
		}
	}
	return m
}
func (m *Matrix) Text(content string, x, y int, options ...Object) (*Matrix, error) {
	return safe(func() *Matrix { return m.text(content, x, y, opt(options)) })
}
func textMatrix(content string, o Object) *Matrix {
	metrics := measureText(content, o["font"], o)
	width, height := integer(number(metrics["width"])), integer(number(metrics["height"]))
	options := Object{}
	for _, k := range []string{"default", "base", "layout"} {
		if has(o, k) {
			options[k] = o[k]
		}
	}
	m := newMatrix(maxInt(1, width), maxInt(1, height), options)
	return m.text(content, 0, 0, join(o, Object{"boxWidth": width}))
}
func TextMatrix(content string, options ...Object) (*Matrix, error) {
	return safe(func() *Matrix { return textMatrix(content, opt(options)) })
}
func init() {
	defineFont(loadData("data/fonts/font-3x5.json"))
	defineFont(loadData("data/fonts/font-5x7.json"))
	Font3x5 = getFont("3x5")
	Font5x7 = getFont("5x7")
}
