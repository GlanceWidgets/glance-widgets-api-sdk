#pragma once
#include "component_options.hpp"
namespace glance_widgets {
inline const Json LINE_CHART_DEFAULTS =
    Json::parse("{\"id\":\"lineChart\",\"interpolation\":\"monotone\",\"strokeWidth\":2,\"fillOpacit"
                "y\":0,\"areaFillOpacity\":0.3,\"markerRadius\":3,\"fontSizeRatio\":0.06,\"axisWi"
                "dth\":1,\"gridCellSize\":8,\"zIndex\":0,\"seriesColors\":[\"#6BA7FF\",\"#FF9F0A\",\""
                "#34C759\",\"#FF453A\",\"#BF5AF2\",\"#5AC8FA\"],\"axisColor\":\"#FFFFFF\",\"labelColo"
                "r\":\"#FFFFFF\",\"axisOpacity\":0.5,\"labelOpacity\":0.72}");
struct LineSeries {
  std::vector<std::pair<double, double>> points;
  std::optional<std::string> color;
  std::optional<std::vector<double>> values;
  LineSeries() = default;
  LineSeries(std::vector<std::pair<double, double>> samples, std::optional<std::string> ink = {})
      : points(std::move(samples)), color(std::move(ink)) {}
  LineSeries(std::vector<double> samples) : values(std::move(samples)) {}
};
using SeriesInput = LineSeries;
struct LineChartMarkers {
  bool last = false, min = false, max = false, labels = false;
  double radius = 3;
};
struct LineChartGrid {
  double cell_size = 8;
  PixelTheme theme = themes.lcd;
};
struct LineChartColors {
  std::vector<std::string> series{"#6BA7FF", "#FF9F0A", "#34C759", "#FF453A", "#BF5AF2", "#5AC8FA"};
  std::optional<std::string> axis, label;
};
struct LineChartOptions : ComponentOptions {
  std::optional<double> min, max;
  std::optional<int> decimals;
  double stroke_width = 2, fill_opacity = 0, axis_width = 1;
  std::string interpolation = "monotone";
  bool show_axes = true, show_value_labels = true;
  std::variant<bool, LineChartGrid> grid = false;
  LineChartMarkers markers;
  std::vector<std::string> x_labels;
  LineChartColors colors;
};
namespace line_chart_detail {
struct Series {
  std::vector<std::pair<double, double>> points;
  std::string color;
  Json markers;
};
struct Plot {
  Canvas canvas;
  Rect area, plot;
  Frame frame;
  double ratio, pt, line_height, gap, axis_x;
};
inline std::vector<std::pair<double, double>> points(const Json &input) {
  auto spec = input.is_array() ? Json::object({{"values", input}}) : input;
  bool values = options_detail::present(spec, "values"), points = options_detail::present(spec, "points");
  if (values == points)
    throw GlanceError("Series needs exactly one of values or points");
  const auto &raw = values ? spec["values"] : spec["points"];
  if (!raw.is_array() || raw.size() < 2 || raw.size() > 1024)
    throw GlanceError("Series needs 2..1024 points");
  std::vector<std::pair<double, double>> out;
  for (std::size_t i = 0; i < raw.size(); ++i) {
    const auto &v = raw.p->items[i];
    if (points && (!v.is_array() || v.size() != 2))
      throw GlanceError("Point must be a pair");
    double x = values ? static_cast<double>(i) : v[0].number(), y = values ? v.number() : v[1].number();
    if (!std::isfinite(x) || !std::isfinite(y))
      throw GlanceError("Series coordinates must be finite");
    out.emplace_back(x, y);
  }
  std::stable_sort(out.begin(), out.end(), [](auto &a, auto &b) { return a.first < b.first; });
  for (std::size_t i = 1; i < out.size(); ++i)
    if (out[i].first == out[i - 1].first)
      throw GlanceError("Repeated x coordinate");
  return out;
}
inline std::vector<std::pair<double, double>> checked_points(std::vector<std::pair<double, double>> points) {
  if (points.size() < 2 || points.size() > 1024)
    throw GlanceError("Series needs 2..1024 points");
  for (auto [x, y] : points)
    if (!std::isfinite(x) || !std::isfinite(y))
      throw GlanceError("Series coordinates must be finite");
  std::stable_sort(points.begin(), points.end(), [](auto a, auto b) { return a.first < b.first; });
  for (std::size_t i = 1; i < points.size(); ++i)
    if (points[i].first == points[i - 1].first)
      throw GlanceError("Repeated x coordinate");
  return points;
}
inline Json markers(const Series &s, const LineChartMarkers &o, int decimals, double radius, const std::string &color) {
  using namespace component_detail;
  std::vector<std::pair<std::size_t, std::string>> wanted;
  std::vector<std::size_t> seen;
  detail::Args out;
  if (o.last)
    wanted.emplace_back(s.points.size() - 1, "last");
  auto cmp = [](auto &a, auto &b) { return a.second < b.second; };
  if (o.max)
    wanted.emplace_back(
        static_cast<std::size_t>(std::max_element(s.points.begin(), s.points.end(), cmp) - s.points.begin()), "max");
  if (o.min)
    wanted.emplace_back(
        static_cast<std::size_t>(std::min_element(s.points.begin(), s.points.end(), cmp) - s.points.begin()), "min");
  for (const auto &wanted_point : wanted) {
    auto index = wanted_point.first;
    if (std::find(seen.begin(), seen.end(), index) != seen.end())
      continue;
    seen.push_back(index);
    auto [x, y] = s.points[index];
    auto at = Json::object({{"x", x}});
    auto marker = Json::object({{"at", at}, {"symbol", Json::object({{"radiusPt", radius}, {"fill", s.color}})}});
    if (o.labels) {
      bool below = wanted_point.second == "min";
      marker["label"] = format(y, decimals);
      marker["labelStyle"] =
          Json::object({{"color", color}, {"fontSizePt", 10}, {"position", below ? "bottom" : "top"}});
      at["offsetPt"] = Json::array({0, below ? radius + 2 : -(radius + 2)});
      marker["at"] = at;
    }
    out.push_back(marker);
  }
  return Json::array(out);
}
inline Plot plot(const LineChartOptions &o, const std::vector<std::string> &ticks, double padding, bool axes,
                 bool labels) {
  using namespace component_detail;
  auto c = canvas(o);
  double ratio = o.font_size_ratio.value_or(0.06), pt = font_pt(ratio, c), line = pt * 1.2, gap = pt * 0.4;
  double aw = o.axis_width, axis = axes ? aw : 0, label_width = 0;
  for (const auto &t : ticks)
    label_width = std::max(label_width, text_width(t, pt));
  double half = labels ? (line * 1.5) / 2 : 0, xrow = o.x_labels.size() > 0 ? gap + text_height(pt) : 0;
  auto inset = [&](double need) { return std::max(need, padding) - padding; };
  auto a = area(o, c);
  auto b = a.inset_pt(
      {inset((labels ? label_width + gap : 0) + axis), inset(half), inset(0), inset(std::max(half, axis + xrow))});
  auto r = emitted(b, c);
  Rect data{r.x + padding, r.y + padding, r.width - 2 * padding, r.height - 2 * padding};
  if (!(data.width > 0) || !(data.height > 0))
    throw GlanceError("Chart area too small for labels");
  return {c, a.to_pt(), data, b.frame(), ratio, pt, line, gap, data.x - aw / 2};
}
inline void overlays(detail::Args &layers, const std::string &id, double z, const LineChartOptions &o, const Plot &l,
                     const std::vector<std::string> &ticks, bool axes, bool labels) {
  using namespace component_detail;
  const auto &p = l.plot;
  auto c = l.canvas;
  double aw = o.axis_width, bottom = p.y + p.height;
  if (axes) {
    AxisLineOptions a;
    a.z_index = z;
    a.style.color = o.colors.axis.value_or("#FFFFFF");
    a.style.stroke_width = aw;
    a.style.opacity = 0.5;
    a.id = id + ":axis-value";
    a.at = l.axis_x / c.width;
    a.from = p.y / c.height;
    a.to = bottom / c.height;
    layers.push_back(vertical_line(a));
    a.id = id + ":axis-base";
    a.at = (bottom + aw / 2) / c.height;
    a.from = (l.axis_x - aw / 2) / c.width;
    a.to = (p.x + p.width) / c.width;
    layers.push_back(horizontal_line(a));
  }
  auto add = [&](const std::string &role, Rect r, const std::string &content, const std::string &h,
                 const std::string &v) {
    layers.push_back(label(id + ":" + role, Box::from_pt(r, c), content, h, v, l.ratio,
                           o.colors.label.value_or("#FFFFFF"), 0.72, z));
  };
  if (labels) {
    double h = l.line_height * 1.5, right = l.axis_x - aw / 2 - l.gap;
    for (std::size_t i = 0; i < ticks.size(); ++i)
      add("value-" + std::to_string(i), {l.area.x, (i == 0 ? p.y : bottom) - h / 2, std::max(0.0, right - l.area.x), h},
          ticks[i], "right", "center");
  }
  auto xlabels = o.x_labels;
  for (std::size_t i = 0; i < xlabels.size(); ++i) {
    auto s = xlabels[i];
    double f = xlabels.size() == 1 ? 0.5 : double(i) / double(xlabels.size() - 1), w = text_width(s, l.pt);
    double x = std::min(l.area.x + l.area.width - w, std::max(l.area.x, p.x + f * p.width - w / 2));
    add("x-" + std::to_string(i), {x, bottom + (axes ? aw : 0) + l.gap, w, text_height(l.pt)}, s, "center", "top");
  }
}
inline Json build(const std::vector<LineSeries> &input, const LineChartOptions &o) {
  using namespace component_detail;
  if (input.empty())
    throw GlanceError("Chart needs at least one series");
  auto id = identity(o, "lineChart");
  double z = o.z_index.value_or(0), stroke = o.stroke_width;
  auto marker_options = o.markers;
  double radius = marker_options.radius;
  bool has_markers = marker_options.last || marker_options.min || marker_options.max;
  auto palette = o.colors.series;
  if (palette.empty())
    throw GlanceError("Series palette must not be empty");
  std::vector<Series> series;
  double xmin = std::numeric_limits<double>::infinity(), xmax = -xmin, low = xmin, high = xmax;
  for (std::size_t i = 0; i < input.size(); ++i) {
    const auto &entry = input[i];
    if (entry.values && !entry.points.empty())
      throw GlanceError("Series needs exactly one of values or points");
    auto samples = entry.points;
    if (entry.values)
      for (std::size_t j = 0; j < entry.values->size(); ++j)
        samples.emplace_back(static_cast<double>(j), (*entry.values)[j]);
    auto pts = checked_points(std::move(samples));
    auto color = normalize_color(entry.color.value_or(palette[i % palette.size()]));
    for (auto [x, y] : pts) {
      xmin = std::min(xmin, x);
      xmax = std::max(xmax, x);
      low = std::min(low, y);
      high = std::max(high, y);
    }
    series.push_back({pts, color, Json::array()});
  }
  low = o.min.value_or(low);
  high = o.max.value_or(high);
  if (low == high && !o.min && !o.max) {
    double pad = std::abs(low) < 0.001 ? 1 : std::abs(low) * 0.1;
    low -= pad;
    high += pad;
  }
  range(low, high);
  int decimals = options_detail::integer(
      o.decimals.value_or(std::floor(low) == low && std::floor(high) == high ? 0 : 1), "decimals", 0, 100);
  double reach = 0;
  auto label_color = o.colors.label.value_or("#FFFFFF");
  for (auto &s : series) {
    for (auto &point : s.points)
      point.second = std::clamp(point.second, low, high);
    s.markers = markers(s, marker_options, decimals, radius, label_color);
    for (const auto &m : s.markers.p->items)
      if (present(m, "label"))
        reach = std::max({reach, radius + 2 + text_height(10), text_width(m["label"].string(), 10) / 2});
  }
  double padding = std::min(48.0, std::max({stroke / 2, has_markers ? radius : 0, reach}));
  bool has_grid = std::holds_alternative<LineChartGrid>(o.grid) || std::get<bool>(o.grid);
  auto grid_options = std::holds_alternative<LineChartGrid>(o.grid) ? std::get<LineChartGrid>(o.grid) : LineChartGrid{};
  bool axes = o.show_axes, labels = o.show_value_labels;
  std::vector<std::string> ticks{format(high, decimals), format(low, decimals)};
  std::optional<Plot> layout;
  if (axes || labels || o.x_labels.size() > 0 || has_grid)
    layout = plot(o, ticks, padding, axes, labels);
  Frame frame = layout ? layout->frame : o.frame.value_or(Frame{0, {}, 0, {}, 1, 1});
  detail::Args layers;
  if (layout && has_grid) {
    auto t = grid_options.theme;
    auto b = Box::from_pt(layout->plot, layout->canvas);
    auto r = emitted(b, layout->canvas);
    SolveGridOptions solve;
    solve.region = r;
    solve.cell_size = grid_options.cell_size;
    solve.gap_mode = t.layout.gap_mode;
    auto solved = solve_grid(solve);
    auto m = matrix({solved.columns, solved.rows, solved.layout}, t, t.default_style);
    auto placed = place_part(m.to_part(), id.first + ":grid", id.second, b, z);
    layers.push_back(placed);
    layout->plot = r;
  }
  for (std::size_t i = 0; i < series.size(); ++i) {
    auto &s = series[i];
    detail::Args pts;
    for (auto [x, y] : s.points)
      pts.push_back(Json::array({x, y}));
    CurveOptions curve_options;
    curve_options.id = id.first + ":series-" + std::to_string(i);
    curve_options.frame = frame;
    curve_options.z_index = z + 1;
    curve_options.data = Json::object({{"mode", "samples"},
                                       {"points", Json::array(pts)},
                                       {"domain", Json::array({xmin, xmax})},
                                       {"yDomain", Json::array({low, high})},
                                       {"interpolation", o.interpolation},
                                       {"markers", s.markers}});
    curve_options.style.color = s.color;
    curve_options.style.fill_color = s.color;
    curve_options.style.fill_opacity = o.fill_opacity;
    curve_options.style.opacity = 1;
    curve_options.style.stroke_width = stroke;
    curve_options.style.plot_padding = padding;
    layers.push_back(curve(curve_options));
  }
  if (layout)
    overlays(layers, id.first, z + 2, o, *layout, ticks, axes, labels);
  return composite(layers);
}
} // namespace line_chart_detail
/** Draw series with deterministic ordering, optional axes, markers and grid. */
inline Json line_chart(const std::vector<LineSeries> &series, const LineChartOptions &o = {}) {
  return line_chart_detail::build(series, o);
}
struct AreaChartOptions : LineChartOptions {
  AreaChartOptions() { fill_opacity = 0.3; }
};
inline Json area_chart(const std::vector<LineSeries> &series, const AreaChartOptions &o = {}) {
  return line_chart(series, o);
}
struct SparklineOptions : LineChartOptions {
  std::optional<std::string> color;
  bool fill = false;
  SparklineOptions() {
    id = "sparkline";
    markers.last = true;
    fill_opacity = 0.3;
    show_axes = false;
    show_value_labels = false;
  }
};
inline Json sparkline(const std::vector<double> &values, const SparklineOptions &input = {}) {
  LineSeries series;
  series.color = input.color;
  for (std::size_t i = 0; i < values.size(); ++i)
    series.points.emplace_back(static_cast<double>(i), values[i]);
  LineChartOptions o = input;
  o.show_axes = false;
  o.show_value_labels = false;
  if (!input.fill)
    o.fill_opacity = 0;
  return line_chart({series}, o);
}
namespace line_chart_detail {
inline LineChartOptions options(const Json &input) {
  using namespace component_detail;
  LineChartOptions o;
  static_cast<ComponentOptions &>(o) = common_options(input);
  o.min = geometry_detail::number(input, "min");
  o.max = geometry_detail::number(input, "max");
  if (present(input, "decimals"))
    o.decimals = options_detail::integer(number(input, "decimals"), "decimals", 0, 100);
  o.stroke_width = number(input, "strokeWidth", 2);
  o.fill_opacity = number(input, "fillOpacity");
  o.axis_width = number(input, "axisWidth", 1);
  o.interpolation = string(input, "interpolation", "monotone");
  o.show_axes = boolean(input, "showAxes", true);
  o.show_value_labels = boolean(input, "showValueLabels", true);
  const auto &grid = input["grid"];
  o.grid = grid.is_object() || (grid.is_bool() && grid.boolean());
  if (grid.is_object())
    o.grid = LineChartGrid{number(grid, "cellSize", 8), theme(grid)};
  const auto &m = input["markers"];
  o.markers = {boolean(m, "last"), boolean(m, "min"), boolean(m, "max"), boolean(m, "labels"), number(m, "radius", 3)};
  if (present(input, "xLabels"))
    for (const auto &label : detail::iter(input["xLabels"]))
      o.x_labels.push_back(label.string());
  const auto &colors = input["colors"];
  if (present(colors, "series")) {
    o.colors.series.clear();
    for (const auto &c : detail::iter(colors["series"]))
      o.colors.series.push_back(c.string());
  }
  o.colors.axis = layers_detail::optional_string(colors, "axis");
  o.colors.label = layers_detail::optional_string(colors, "label");
  return o;
}
inline std::vector<LineSeries> series(const Json &input) {
  std::vector<LineSeries> result;
  for (const auto &entry : detail::iter(input))
    result.push_back({points(entry), layers_detail::optional_string(entry, "color")});
  return result;
}
} // namespace line_chart_detail
inline Json line_chart(const Json &series, const Json &o = Json::object()) {
  return line_chart(line_chart_detail::series(series), line_chart_detail::options(o));
}
inline Json area_chart(const Json &series, const Json &input = Json::object()) {
  auto o = line_chart_detail::options(input);
  o.fill_opacity = options_detail::number(input, "fillOpacity", 0.3);
  return line_chart(line_chart_detail::series(series), o);
}
inline Json sparkline(const Json &values, const Json &input = Json::object()) {
  SparklineOptions o;
  static_cast<LineChartOptions &>(o) = line_chart_detail::options(input);
  o.id = options_detail::string(input, "id", "sparkline");
  if (!options_detail::present(input, "markers"))
    o.markers.last = true;
  o.color = layers_detail::optional_string(input, "color");
  o.fill = options_detail::boolean(input, "fill");
  o.fill_opacity = options_detail::number(input, "fillOpacity", 0.3);
  std::vector<double> points;
  for (const auto &v : detail::iter(values))
    points.push_back(v.number());
  return sparkline(points, o);
}
} // namespace glance_widgets
