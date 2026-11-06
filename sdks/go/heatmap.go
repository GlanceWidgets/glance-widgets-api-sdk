package glance

import (
	"math"
	"regexp"
	"time"
)

var HeatmapDefaults = Object{"missing": "empty", "calendarLevels": 5.}
var CalendarHeatmapDefaults = Object{"startWeekday": 0., "orientation": "weekColumns", "weekdayLabels": []string{"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}, "monthLabels": []string{"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"}}
var LabeledCalendarHeatmapDefaults = Object{"id": "calendar", "fontSizeRatio": .05, "labelOpacity": .72, "zIndex": 0.}
var isoDatePattern = regexp.MustCompile(`^[0-9]{4}-[0-9]{2}-[0-9]{2}$`)

func heatScale(values []any, theme, o Object, defaultLevels float64) func(any) any {
	stops := []string{s(object(theme["palette"]), "off", ""), s(object(theme["palette"]), "on", "")}
	if has(o, "scale") {
		stops = stringsOf(o["scale"])
	}
	if len(stops) < 2 {
		fail("heat scale needs at least two stops")
	}
	levels := n(o, "levels", defaultLevels)
	if has(o, "levels") || defaultLevels > 0 {
		if integer(levels) < 2 {
			fail("levels must be integer of at least2")
		}
	}
	present := []float64{}
	for _, v := range values {
		if v != nil {
			present = append(present, finite(number(v)))
		}
	}
	lo, hi := 0., 0.
	if len(present) > 0 {
		lo, hi = present[0], present[0]
		for _, v := range present {
			lo = math.Min(lo, v)
			hi = math.Max(hi, v)
		}
	}
	lo, hi = n(o, "min", lo), n(o, "max", hi)
	if hi < lo {
		fail("heat max must not be below min")
	}
	return func(value any) any {
		if value == nil {
			return nil
		}
		t := 0.
		if hi > lo {
			t = clamp((number(value)-lo)/(hi-lo), 0, 1)
		}
		if levels > 0 {
			t = math.Min(levels-1, math.Floor(t*levels)) / (levels - 1)
		}
		position := t * float64(len(stops)-1)
		index := minInt(len(stops)-2, int(math.Floor(position)))
		return mixColors(stops[index], stops[index+1], position-float64(index))
	}
}
func paintHeatCell(m *Matrix, x, y int, value any, colorOf func(any) any, missing string) {
	color := colorOf(value)
	if color != nil {
		m.set(x, y, color)
	} else if missing != "empty" {
		m.set(x, y, missing)
	}
}
func heatmap(values []any, o Object) Object {
	rows, columns := len(values), 0
	if rows > 0 {
		columns = len(array(values[0]))
	}
	if rows == 0 || columns == 0 {
		fail("heatmap needs rows and columns")
	}
	flat := []any{}
	for _, row := range values {
		a := array(row)
		if len(a) != columns {
			fail("heatmap rows must match")
		}
		flat = append(flat, a...)
	}
	theme := resolveTheme(o["theme"])
	colors := heatScale(flat, theme, o, 0)
	plan := planGrid(regionOf(o, "heatmap"), Object{"columns": columns, "rows": rows}, theme, "heatmap")
	m := newMatrix(columns, rows, themeOptions(theme, Object{"default": Empty, "layout": plan["layout"]}))
	for y, row := range values {
		for x, value := range array(row) {
			paintHeatCell(m, x, y, value, colors, s(o, "missing", "empty"))
		}
	}
	return m.toPart()
}

type calendarSlot struct{ Week, Day int }
type calendarLayout struct {
	Days                []time.Time
	Slots               []calendarSlot
	Weeks, StartWeekday int
	Vertical            bool
}

func calendarPlan(count int, o Object) calendarLayout {
	if count == 0 {
		fail("calendar needs at least one day")
	}
	weekday := integer(n(o, "startWeekday", 0))
	if weekday < 0 || weekday > 6 {
		fail("startWeekday must be0..6")
	}
	orientation := s(o, "orientation", "weekColumns")
	if orientation != "weekColumns" && orientation != "weekRows" {
		fail("invalid calendar orientation")
	}
	start := s(o, "start", "")
	if !isoDatePattern.MatchString(start) {
		fail("calendar start must be YYYY-MM-DD")
	}
	begin, err := time.Parse("2006-01-02", start)
	if err != nil || begin.Year() < 100 {
		fail("calendar start is not a real supported date")
	}
	days := make([]time.Time, count)
	slots := make([]calendarSlot, count)
	leading := (int(begin.Weekday()) - weekday + 7) % 7
	for i := 0; i < count; i++ {
		days[i] = begin.AddDate(0, 0, i)
		slots[i] = calendarSlot{(leading + i) / 7, (leading + i) % 7}
	}
	return calendarLayout{days, slots, slots[count-1].Week + 1, weekday, orientation == "weekRows"}
}
func calendarMatrix(values []any, layout calendarLayout, o Object) *Matrix {
	theme := resolveTheme(o["theme"])
	colors := heatScale(values, theme, o, 5)
	columns, rows := layout.Weeks, 7
	if layout.Vertical {
		columns, rows = 7, layout.Weeks
	}
	plan := planGrid(regionOf(o, "calendarHeatmap"), Object{"columns": columns, "rows": rows}, theme, "calendarHeatmap")
	m := newMatrix(columns, rows, themeOptions(theme, Object{"default": Empty, "layout": plan["layout"]}))
	for i, value := range values {
		slot := layout.Slots[i]
		x, y := slot.Week, slot.Day
		if layout.Vertical {
			x, y = y, x
		}
		paintHeatCell(m, x, y, value, colors, s(o, "missing", "empty"))
	}
	return m
}
func calendarHeatmap(days []any, o Object) Object {
	return calendarMatrix(days, calendarPlan(len(days), o), o).toPart()
}

type monthWeek struct{ Week, Month int }

func monthLabelWeeks(layout calendarLayout, minGap int) []monthWeek {
	labels := []monthWeek{}
	for i, day := range layout.Days {
		week := layout.Slots[i].Week
		if day.Day() == 1 && (len(labels) == 0 || week-labels[len(labels)-1].Week >= minGap) {
			labels = append(labels, monthWeek{week, int(day.Month()) - 1})
		}
	}
	return labels
}
func labeledCalendarHeatmap(days []any, o Object) Object {
	c := canvasOf(o["canvas"])
	id, path := identity(o, "calendar", "labeledCalendarHeatmap")
	theme := resolveTheme(o["theme"])
	layout := calendarPlan(len(days), o)
	weekdayNames, monthNames := stringsOf(CalendarHeatmapDefaults["weekdayLabels"]), stringsOf(CalendarHeatmapDefaults["monthLabels"])
	if has(o, "weekdayLabels") {
		weekdayNames = stringsOf(o["weekdayLabels"])
	}
	if has(o, "monthLabels") {
		monthNames = stringsOf(o["monthLabels"])
	}
	if len(weekdayNames) != 7 || len(monthNames) != 12 {
		fail("calendar needs7 weekday and12 month names")
	}
	weekdays, months := b(o, "showWeekdayLabels", true), b(o, "showMonthLabels", true)
	z, ratio := n(o, "zIndex", 0), n(o, "fontSizeRatio", .05)
	pt := fontPt(ratio, c)
	rowPt, gapPt := pt*1.2*1.3, pt*.4
	widest := func(names []string) float64 {
		width := 0.
		for _, name := range names {
			width = math.Max(width, estimateTextWidth(name, pt))
		}
		return width
	}
	sideReserve, topReserve := 0., 0.
	if weekdays && !layout.Vertical {
		sideReserve += widest(weekdayNames)
	}
	if months && layout.Vertical {
		sideReserve += widest(monthNames)
	}
	if weekdays && layout.Vertical || months && !layout.Vertical {
		topReserve = rowPt
	}
	area := resolveArea(o["frame"], c)
	r := area.toPt()
	leftInset, topInset := 0., 0.
	if sideReserve > 0 {
		leftInset = sideReserve + gapPt
	}
	if topReserve > 0 {
		topInset = topReserve + gapPt
	}
	gridBox := area.insetPt(Object{"left": leftInset, "top": topInset})
	frame, rect := emittedRegion(gridBox, c)
	m := calendarMatrix(days, layout, join(o, Object{"frame": gridBox, "canvas": c}))
	placed := place(m.toPart(), Object{"id": id + ":grid", "frame": frame, "path": path, "zIndex": z})
	geometry := fitMatrix(rect, Object{"columns": m.Columns, "rows": m.Rows}, m.Layout)
	layers := []any{placed}
	labelColor := object(theme["palette"])["on"]
	if v, ok := object(o["colors"])["label"]; ok && v != nil {
		labelColor = v
	}
	add := func(role string, rect Rect, content, horizontal string) {
		layers = append(layers, label(id+":"+role, boxFromPt(rect, c), content, horizontal, "center", ratio, labelColor, .72, z+1))
	}
	left := func(x, w float64) float64 { return math.Max(r.X, math.Min(x, r.X+r.Width-w)) }
	top := func(y, h float64) float64 { return math.Max(r.Y, math.Min(y, r.Y+r.Height-h)) }
	if weekdays {
		for day := 1; day < 7; day += 2 {
			name := weekdayNames[(layout.StartWeekday+day)%7]
			width := estimateTextWidth(name, pt)
			if layout.Vertical {
				cell := cellRect(geometry, float64(day), 0)
				add("weekday-"+formatNumber(float64(day), 0), Rect{left(cell.X+cell.Width/2-width/2, width), r.Y, width, rowPt}, name, "center")
			} else {
				cell := cellRect(geometry, 0, float64(day))
				add("weekday-"+formatNumber(float64(day), 0), Rect{r.X, top(cell.Y+cell.Height/2-rowPt/2, rowPt), widest(weekdayNames), rowPt}, name, "right")
			}
		}
	}
	if months {
		pitch := n(geometry, "cellWidth", 0) + n(geometry, "gapX", 0)
		span := widest(monthNames)
		if layout.Vertical {
			pitch = n(geometry, "cellHeight", 0) + n(geometry, "gapY", 0)
			span = rowPt
		}
		minGap := maxInt(1, toCount(math.Ceil(span/pitch)))
		labels := monthLabelWeeks(layout, minGap)
		for _, entry := range labels {
			week, name := entry.Week, monthNames[entry.Month]
			role := "month-" + formatNumber(float64(week), 0)
			if layout.Vertical {
				cell := cellRect(geometry, 0, float64(week))
				add(role, Rect{r.X, top(cell.Y+cell.Height/2-rowPt/2, rowPt), widest(monthNames), rowPt}, name, "right")
			} else {
				cell := cellRect(geometry, float64(week), 0)
				width := estimateTextWidth(name, pt)
				add(role, Rect{left(cell.X, width), r.Y, width, rowPt}, name, "left")
			}
		}
	}
	return combineComponents(layers)
}
func Heatmap(values any, options ...Object) (MatrixPart, error) {
	return safe(func() Object { return heatmap(array(values), opt(options)) })
}
func CalendarHeatmap(days []any, o Object) (MatrixPart, error) {
	return safe(func() Object { return calendarHeatmap(days, o) })
}
func LabeledCalendarHeatmap(days []any, o Object) (Composite, error) {
	return safe(func() Object { return labeledCalendarHeatmap(days, o) })
}
