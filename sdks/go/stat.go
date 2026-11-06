package glance

import "math"

var StatDefaults = Object{"id": "stat", "deltaDecimals": 1., "arrow": "icon", "valueStyle": "text", "align": "left", "fontSizeRatio": .08, "pixelFont": "5x7", "pixelUnitFont": "3x5", "zIndex": 0., "labelOpacity": .72, "valueColor": "#FFFFFF", "labelColor": "#FFFFFF", "upIcon": "ph_arrowUp", "downIcon": "ph_arrowDown", "flatIcon": "ph_minus"}

func arrowMatrix(direction, color string, o Object) *Matrix {
	rows := []string{"..#..", ".###.", "#####", "..#..", "..#.."}
	if direction == "down" {
		for i, j := 0, len(rows)-1; i < j; i, j = i+1, j-1 {
			rows[i], rows[j] = rows[j], rows[i]
		}
	} else if direction == "flat" {
		rows = []string{".....", ".....", "#####", ".....", "....."}
	}
	m := contentMatrix(5, 5, resolveTheme(o["theme"]))
	for y, row := range rows {
		for x, ch := range row {
			if ch == '#' {
				m.set(x, y, color)
			}
		}
	}
	return m
}
func stat(value any, o Object) Object {
	c := canvasOf(o["canvas"])
	id, path := identity(o, "stat", "stat")
	valueText, isString := value.(string)
	if !isString {
		v := finite(number(value))
		valueText = formatNumber(v, decimalsOf(n(o, "decimals", float64(defaultDecimals(v)))))
	}
	theme := resolveTheme(o["theme"])
	pixel, center, z := s(o, "valueStyle", "text") == "pixel", s(o, "align", "left") == "center", n(o, "zIndex", 0)
	colors := object(o["colors"])
	valueColor := "#FFFFFF"
	if pixel {
		valueColor = s(object(theme["palette"]), "on", "")
	}
	valueColor = s(colors, "value", valueColor)
	direction := ""
	if has(o, "delta") {
		d := n(o, "delta", 0)
		direction = "flat"
		if d > 0 {
			direction = "up"
		} else if d < 0 {
			direction = "down"
		}
	}
	deltaContent, hasDelta := "", has(o, "deltaText") || has(o, "delta")
	if has(o, "deltaText") {
		deltaContent = s(o, "deltaText", "")
	} else if has(o, "delta") {
		d := n(o, "delta", 0)
		if d > 0 {
			deltaContent = "+"
		}
		deltaContent += formatNumber(d, decimalsOf(n(o, "deltaDecimals", 1))) + s(o, "deltaSuffix", "")
	}
	arrowKind := s(o, "arrow", "icon")
	present := []string{}
	weights := []any{}
	if has(o, "label") {
		present = append(present, "label")
		weights = append(weights, 2.)
	}
	present = append(present, "value")
	weights = append(weights, 5.)
	if hasDelta {
		present = append(present, "delta")
		weights = append(weights, 2.)
	}
	boxes := resolveArea(o["frame"], c).split("rows", weights, Object{})
	rowBox := func(name string) Rect {
		for i, p := range present {
			if p == name {
				return boxes[i].toPt()
			}
		}
		fail("missing stat row")
		return Rect{}
	}
	layers := []any{}
	horizontal := "left"
	if center {
		horizontal = "center"
	}
	textLayer := func(role string, r Rect, content, horizontal string, pt float64, color string, opacity float64, weight string) Object {
		return label(id+":"+role, boxFromPt(r, c), content, horizontal, "center", fontRatio(pt, c), color, opacity, z, weight)
	}
	usedPt := func(pt float64) float64 { return fontPt(fontRatio(pt, c), c) }
	fitFont := func(height, width float64, content string) float64 {
		return usedPt(math.Min(height/labelBoxEm, math.Min(fontPtForWidth(content, width), fontPt(n(o, "fontSizeRatio", .08), c))))
	}
	if has(o, "label") {
		r := rowBox("label")
		content := s(o, "label", "")
		pt := fitFont(r.Height, r.Width, content)
		layers = append(layers, textLayer("label", r, content, horizontal, pt, s(colors, "label", "#FFFFFF"), .72, "400"))
	}
	valueRect := rowBox("value")
	if pixel {
		font := o["pixelFont"]
		if font == nil {
			font = "5x7"
		}
		main := measureText(valueText, font, Object{})
		mw, mh := integer(number(main["width"])), integer(number(main["height"]))
		smallW, smallH := 0, 0
		unitFont := o["pixelUnitFont"]
		if unitFont == nil {
			unitFont = "3x5"
		}
		if has(o, "unit") {
			small := measureText(s(o, "unit", ""), unitFont, Object{})
			smallW, smallH = integer(number(small["width"])), integer(number(small["height"]))
		}
		width := mw
		if has(o, "unit") {
			width += 1 + smallW
		}
		content := contentMatrix(width, maxInt(mh, smallH), theme)
		content.text(valueText, 0, 0, Object{"font": font, "style": valueColor})
		if has(o, "unit") {
			content.text(s(o, "unit", ""), mw+1, maxInt(0, mh-smallH), Object{"font": unitFont, "style": s(colors, "value", s(object(theme["palette"]), "dim", ""))})
		}
		frame, rect := emittedRegion(boxFromPt(valueRect, c), c)
		fitOptions := Object{"theme": theme, "align": horizontal, "valign": "center"}
		if has(o, "maxCellSize") {
			fitOptions["maxCellSize"] = o["maxCellSize"]
		}
		m := fitPixelContent(content, rect, fitOptions)
		placed := place(m.toPart(), Object{"id": id + ":value", "frame": frame, "path": path, "zIndex": z})
		layers = append(layers, placed)
	} else {
		unit := s(o, "unit", "")
		valueEm := textAdvanceEm(valueText)
		widthEm := valueEm * 1.05
		if has(o, "unit") {
			widthEm = math.Max(valueEm*1.05, valueEm+.15+textAdvanceEm(unit)*1.05*.45)
		}
		fitWidth := math.Max(0, valueRect.Width-1) / math.Max(1e-9, widthEm)
		pt := usedPt(math.Min(valueRect.Height/1.32, fitWidth))
		valueWidth := textAdvancePt(valueText, pt)
		unitFont := usedPt(pt * .45)
		unitGap := pt * .15
		unitWidth := 0.
		if has(o, "unit") {
			unitWidth = textAdvancePt(unit, unitFont)
		}
		total := valueWidth
		if has(o, "unit") {
			total += unitGap + unitWidth
		}
		start := valueRect.X
		if center {
			start += (valueRect.Width - total) / 2
		}
		boxHeight := pt * 1.32
		top := valueRect.Y + (valueRect.Height-boxHeight)/2
		layers = append(layers, textLayer("value", Rect{start, top, estimateTextWidth(valueText, pt), boxHeight}, valueText, "left", pt, valueColor, 1, "700"))
		if has(o, "unit") {
			height := unitFont * 1.32
			layers = append(layers, textLayer("unit", Rect{start + valueWidth + unitGap, top + boxHeight - height, estimateTextWidth(unit, unitFont), height}, unit, "left", unitFont, valueColor, .72, "400"))
		}
	}
	if hasDelta {
		r := rowBox("delta")
		status := "neutral"
		if direction != "" && direction != "flat" {
			good := (direction == "up") != b(o, "invertDelta", false)
			status = "bad"
			if good {
				status = "good"
			}
		}
		color := s(colors, status, s(StatusColors, status, ""))
		showArrow := arrowKind != "none" && direction != ""
		available := r.Width
		if showArrow {
			available -= 1.2 * r.Height
		}
		pt := fitFont(r.Height, available, deltaContent)
		arrowPt, gap := 0., 0.
		if showArrow {
			arrowPt, gap = pt*1.2, pt*.4
		}
		total := arrowPt + gap + textAdvancePt(deltaContent, pt)
		start := r.X
		if center {
			start += (r.Width - total) / 2
		}
		if showArrow {
			square := Rect{start, r.Y + (r.Height-arrowPt)/2, arrowPt, arrowPt}
			if arrowKind == "icon" {
				name := "ph_minus"
				if direction == "up" {
					name = "ph_arrowUp"
				} else if direction == "down" {
					name = "ph_arrowDown"
				}
				layers = append(layers, icon(Object{"id": id + ":arrow", "name": name, "frame": boxFromPt(square, c), "zIndex": z, "style": Object{"color": color}}))
			} else {
				frame, _ := emittedRegion(boxFromPt(square, c), c)
				placed := place(arrowMatrix(direction, color, o).toPart(), Object{"id": id + ":arrow", "frame": frame, "path": path + "Arrow", "zIndex": z})
				layers = append(layers, placed)
			}
		}
		layers = append(layers, textLayer("delta", Rect{start + arrowPt + gap, r.Y, estimateTextWidth(deltaContent, pt), r.Height}, deltaContent, "left", pt, color, 1, "600"))
	}
	return combineComponents(layers)
}
func Stat(value any, o Object) (Composite, error) {
	return safe(func() Object { return stat(value, o) })
}
