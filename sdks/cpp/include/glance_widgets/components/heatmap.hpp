#pragma once
#include "component_options.hpp"
namespace glance_widgets {
inline const Json HEATMAP_DEFAULTS = Json::parse(R"({"missing":"empty","calendarLevels":5})");
inline const Json CALENDAR_HEATMAP_DEFAULTS =
    Json::parse("{\"startWeekday\":0,\"orientation\":\"weekColumns\",\"weekdayLabels\":[\"Sun\",\"Mo"
                "n\",\"Tue\",\"Wed\",\"Thu\",\"Fri\",\"Sat\"],\"monthLabels\":[\"Jan\",\"Feb\",\"Mar\",\"Apr\""
                ",\"May\",\"Jun\",\"Jul\",\"Aug\",\"Sep\",\"Oct\",\"Nov\",\"Dec\"]}");
inline const Json LABELED_CALENDAR_HEATMAP_DEFAULTS =
    Json::parse(R"({"id":"calendar","fontSizeRatio":0.05,"labelOpacity":0.72,"zIndex":0})");
using HeatValue = std::optional<double>;
struct HeatScaleOptions {
  std::optional<double> min, max;
  std::optional<int> levels;
  std::vector<std::string> scale;
  std::string missing = "empty";
};
struct HeatmapOptions : ComponentOptions, HeatScaleOptions {};
struct CalendarHeatmapOptions : HeatmapOptions {
  std::string start, orientation = "weekColumns";
  int start_weekday = 0;
};
struct LabeledCalendarHeatmapOptions : CalendarHeatmapOptions {
  bool show_weekday_labels = true, show_month_labels = true;
  std::optional<std::vector<std::string>> weekday_labels, month_labels;
  std::optional<std::string> label_color;
};
namespace heatmap_detail {
struct Scale {
  std::vector<std::string> stops;
  double min = 0, max = 0;
  std::optional<double> levels;
  std::string missing;
  Scale(const std::vector<HeatValue> &values, const PixelTheme &t, const HeatScaleOptions &o, bool calendar) {
    using namespace component_detail;
    stops = o.scale.empty() ? std::vector<std::string>{t.palette.off, t.palette.on} : o.scale;
    if (stops.size() < 2)
      throw GlanceError("heatmap scale needs at least 2 colors");
    levels = o.levels;
    if (!levels && calendar)
      levels = 5;
    if (levels)
      options_detail::integer(*levels, "levels", 2, 2147483647);
    bool first = true;
    for (const auto &v : values)
      if (v.has_value()) {
        if (!std::isfinite(*v))
          throw GlanceError("heatmap values must be finite or null");
        if (first) {
          min = max = *v;
          first = false;
        } else {
          min = std::min(min, *v);
          max = std::max(max, *v);
        }
      }
    min = o.min.value_or(min);
    max = o.max.value_or(max);
    if (!std::isfinite(min) || !std::isfinite(max) || max < min)
      throw GlanceError("max below min");
    missing = o.missing;
  }
  void paint(Matrix &m, double x, double y, const HeatValue &value) const {
    if (!value) {
      if (missing != "empty")
        m.set(x, y, missing);
      return;
    }
    double t = max > min ? std::clamp((*value - min) / (max - min), 0.0, 1.0) : 0;
    if (levels)
      t = std::min(*levels - 1, std::floor(t * *levels)) / (*levels - 1);
    double position = t * static_cast<double>(stops.size() - 1);
    if (!std::isfinite(position))
      throw GlanceError("heatmap scale is not finite");
    auto index = static_cast<std::size_t>(std::min(static_cast<double>(stops.size() - 2), std::floor(position)));
    m.set(x, y, mix_colors(stops[index], stops[index + 1], position - static_cast<double>(index)));
  }
};
struct Day {
  int year, month, day, weekday;
};
inline bool leap(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }
inline int month_days(int y, int m) {
  constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return days[m - 1] + (m == 2 && leap(y) ? 1 : 0);
}
inline bool iso_date_ok(const std::string &s) {
  if (s.size() != 10)
    return false;
  for (std::size_t i = 0; i < 10; ++i)
    if (i == 4 || i == 7 ? s[i] != '-' : !(s[i] >= '0' && s[i] <= '9'))
      return false;
  return true;
}
inline Day start(const std::string &s) {
  if (!iso_date_ok(s))
    throw GlanceError("start must be YYYY-MM-DD");
  int y = std::stoi(s.substr(0, 4)), m = std::stoi(s.substr(5, 2)), d = std::stoi(s.substr(8, 2));
  if (y < 100 || m < 1 || m > 12 || d < 1 || d > month_days(y, m))
    throw GlanceError("Invalid calendar date");
  int previous = y - 1, total = 365 * previous + previous / 4 - previous / 100 + previous / 400;
  for (int i = 1; i < m; ++i)
    total += month_days(y, i);
  return {y, m, d, (total + d) % 7};
}
struct Calendar {
  std::vector<Day> days;
  std::vector<std::pair<int, int>> slots;
  int weeks = 0, start_weekday = 0;
  bool vertical = false;
  Calendar(std::size_t count, const CalendarHeatmapOptions &o) {
    using namespace component_detail;
    if (!count || count > 4096)
      throw GlanceError("calendar needs 1..4096 days");
    start_weekday = options_detail::integer(o.start_weekday, "startWeekday", 0, 6);
    auto orientation = o.orientation;
    vertical = orientation == "weekRows";
    if (!vertical && orientation != "weekColumns")
      throw GlanceError("Invalid calendar orientation");
    auto date = start(o.start);
    int leading = (date.weekday - start_weekday + 7) % 7;
    for (std::size_t i = 0; i < count; ++i) {
      days.push_back(date);
      int slot = leading + static_cast<int>(i);
      slots.emplace_back(slot / 7, slot % 7);
      if (++date.day > month_days(date.year, date.month)) {
        date.day = 1;
        if (++date.month > 12) {
          date.month = 1;
          ++date.year;
        }
      }
      date.weekday = (date.weekday + 1) % 7;
    }
    weeks = slots.back().first + 1;
  }
};
inline Matrix calendar_matrix(const std::vector<HeatValue> &values, const Calendar &cal,
                              const CalendarHeatmapOptions &o, std::optional<Rect> r) {
  using namespace component_detail;
  auto t = o.theme;
  Scale scale(values, t, o, true);
  auto p = plan(r, cal.vertical ? 7 : cal.weeks, cal.vertical ? cal.weeks : 7, t);
  auto m = matrix(p, t);
  for (std::size_t i = 0; i < values.size(); ++i) {
    auto slot = cal.slots[i];
    scale.paint(m, cal.vertical ? slot.second : slot.first, cal.vertical ? slot.first : slot.second, values[i]);
  }
  return m;
}
} // namespace heatmap_detail
/** Render a rectangular grid of optional finite values. */
inline Json heatmap(const std::vector<std::vector<HeatValue>> &values, const HeatmapOptions &o = {}) {
  using namespace component_detail;
  if (values.empty() || values[0].empty())
    throw GlanceError("heatmap needs values");
  auto rows = values.size(), cols = values[0].size();
  std::vector<HeatValue> flat;
  for (const auto &row : values) {
    if (row.size() != cols)
      throw GlanceError("heatmap rows must have equal length");
    flat.insert(flat.end(), row.begin(), row.end());
  }
  auto t = o.theme;
  heatmap_detail::Scale scale(flat, t, o, false);
  auto m = matrix(plan(region(o), static_cast<double>(cols), static_cast<double>(rows), t), t);
  for (std::size_t y = 0; y < rows; ++y)
    for (std::size_t x = 0; x < cols; ++x)
      scale.paint(m, static_cast<double>(x), static_cast<double>(y), values[y][x]);
  return m.to_part();
}
/** Lay daily values into calendar weeks. */
inline Json calendar_heatmap(const std::vector<HeatValue> &days, const CalendarHeatmapOptions &o) {
  heatmap_detail::Calendar cal(days.size(), o);
  return heatmap_detail::calendar_matrix(days, cal, o, component_detail::region(o)).to_part();
}
/** Add month and weekday labels around a calendar grid. */
inline Json labeled_calendar_heatmap(const std::vector<HeatValue> &days, const LabeledCalendarHeatmapOptions &o) {
  using namespace component_detail;
  auto c = canvas(o);
  auto id = identity(o, "calendar");
  auto t = o.theme;
  heatmap_detail::Calendar cal(days.size(), o);
  auto weekdays = o.weekday_labels.value_or(std::vector<std::string>{"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"});
  auto months = o.month_labels.value_or(
      std::vector<std::string>{"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"});
  if (weekdays.size() != 7 || months.size() != 12)
    throw GlanceError("Calendar label count must be 7 and 12");
  bool show_days = o.show_weekday_labels, show_months = o.show_month_labels;
  double z = o.z_index.value_or(0), ratio = o.font_size_ratio.value_or(0.05), pt = font_pt(ratio, c),
         row = pt * 1.2 * 1.3, gap = pt * 0.4;
  auto widest = [&](const std::vector<std::string> &names) {
    double w = 0;
    for (const auto &n : names)
      w = std::max(w, text_width(n, pt));
    return w;
  };
  double side =
      (show_days && !cal.vertical ? widest(weekdays) : 0) + (show_months && cal.vertical ? widest(months) : 0);
  double top = (show_days && cal.vertical) || (show_months && !cal.vertical) ? row : 0;
  auto box = area(o, c);
  auto a = box.to_pt();
  auto grid = box.inset_pt({side > 0 ? side + gap : 0, top > 0 ? top + gap : 0, 0, 0});
  auto rect = emitted(grid, c);
  auto m = heatmap_detail::calendar_matrix(days, cal, o, rect);
  auto placed = place_part(m.to_part(), id.first + ":grid", id.second, grid, z);
  auto g = fit_matrix(rect, GridSize{double(m.columns()), double(m.rows())}, m.layout_options());
  detail::Args layers{placed};
  auto ink = o.label_color.value_or(t.palette.on);
  auto add = [&](std::string role, Rect r, std::string content, std::string align) {
    layers.push_back(
        label(id.first + ":" + role, Box::from_pt(r, c), content, align, "center", ratio, ink, 0.72, z + 1));
  };
  auto left = [&](double x, double w) { return std::max(a.x, std::min(x, a.x + a.width - w)); };
  auto y = [&](double y, double h) { return std::max(a.y, std::min(y, a.y + a.height - h)); };
  if (show_days)
    for (int day = 1; day < 7; day += 2) {
      auto name = weekdays[static_cast<std::size_t>((cal.start_weekday + day) % 7)];
      double w = text_width(name, pt);
      auto cell = cell_rect(g, cal.vertical ? day : 0, cal.vertical ? 0 : day);
      add("weekday-" + std::to_string(day),
          cal.vertical ? Rect{left(cell.x + cell.width / 2 - w / 2, w), a.y, w, row}
                       : Rect{a.x, y(cell.y + cell.height / 2 - row / 2, row), widest(weekdays), row},
          name, cal.vertical ? "center" : "right");
    }
  if (show_months) {
    double pitch = cal.vertical ? g.cell_height + g.gap_y : g.cell_width + g.gap_x;
    double min_gap = std::max(1.0, std::ceil((cal.vertical ? row : widest(months)) / pitch));
    auto select = [&](double gap) {
      std::vector<std::pair<int, int>> selected;
      for (std::size_t i = 0; i < cal.days.size(); ++i)
        if (cal.days[i].day == 1) {
          int week = cal.slots[i].first;
          if (selected.empty() || week - selected.back().first >= gap)
            selected.emplace_back(week, cal.days[i].month - 1);
        }
      return selected;
    };
    auto selected = select(min_gap);
    for (auto [week, month] : selected) {
      auto name = months[static_cast<std::size_t>(month)];
      auto cell = cell_rect(g, cal.vertical ? 0 : week, cal.vertical ? week : 0);
      double w = cal.vertical ? widest(months) : text_width(name, pt);
      add("month-" + std::to_string(week),
          cal.vertical ? Rect{a.x, y(cell.y + cell.height / 2 - row / 2, row), w, row}
                       : Rect{left(cell.x, w), a.y, w, row},
          name, cal.vertical ? "right" : "left");
    }
  }
  return composite(layers);
}
namespace heatmap_detail {
inline std::vector<HeatValue> values(const Json &input) {
  if (!input.is_array())
    throw GlanceError("heatmap needs values");
  std::vector<HeatValue> result;
  for (const auto &v : input.p->items)
    result.push_back(v.is_null() ? HeatValue{} : HeatValue{v.number()});
  return result;
}
inline HeatmapOptions options(const Json &input) {
  using namespace component_detail;
  HeatmapOptions o;
  static_cast<ComponentOptions &>(o) = common_options(input);
  o.min = geometry_detail::number(input, "min");
  o.max = geometry_detail::number(input, "max");
  if (present(input, "levels"))
    o.levels = options_detail::integer(number(input, "levels"), "levels", 2, 2147483647);
  if (present(input, "scale")) {
    for (const auto &v : detail::iter(input["scale"]))
      o.scale.push_back(v.string());
    if (o.scale.size() < 2)
      throw GlanceError("heatmap scale needs at least 2 colors");
  }
  o.missing = string(input, "missing", "empty");
  return o;
}
inline CalendarHeatmapOptions calendar_options(const Json &input) {
  using namespace component_detail;
  CalendarHeatmapOptions o;
  static_cast<HeatmapOptions &>(o) = options(input);
  o.start = string(input, "start");
  o.orientation = string(input, "orientation", "weekColumns");
  o.start_weekday = options_detail::integer(number(input, "startWeekday"), "startWeekday", 0, 6);
  return o;
}
inline LabeledCalendarHeatmapOptions labeled_options(const Json &input) {
  using namespace component_detail;
  LabeledCalendarHeatmapOptions o;
  static_cast<CalendarHeatmapOptions &>(o) = calendar_options(input);
  o.show_weekday_labels = boolean(input, "showWeekdayLabels", true);
  o.show_month_labels = boolean(input, "showMonthLabels", true);
  auto labels = [&](const char *key, std::optional<std::vector<std::string>> &output) {
    if (present(input, key)) {
      output.emplace();
      for (const auto &v : detail::iter(input[key]))
        output->push_back(v.string());
    }
  };
  labels("weekdayLabels", o.weekday_labels);
  labels("monthLabels", o.month_labels);
  o.label_color = layers_detail::optional_string(input["colors"], "label");
  return o;
}
} // namespace heatmap_detail
inline Json heatmap(const Json &input, const Json &o = Json::object()) {
  std::vector<std::vector<HeatValue>> rows;
  for (const auto &row : detail::iter(input))
    rows.push_back(heatmap_detail::values(row));
  return heatmap(rows, heatmap_detail::options(o));
}
inline Json calendar_heatmap(const Json &days, const Json &o) {
  return calendar_heatmap(heatmap_detail::values(days), heatmap_detail::calendar_options(o));
}
inline Json labeled_calendar_heatmap(const Json &days, const Json &o) {
  return labeled_calendar_heatmap(heatmap_detail::values(days), heatmap_detail::labeled_options(o));
}
} // namespace glance_widgets
