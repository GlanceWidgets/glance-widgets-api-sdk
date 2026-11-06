#pragma once
#include "component_options.hpp"
namespace glance_widgets {
inline const Json PIXEL_BAR_CHART_DEFAULTS =
    Json::parse("{\"id\":\"pixelBarChart\",\"rows\":10,\"gap\":0.16,\"barWidth\":1,\"barSpacing\":1,\"corne"
                "rRadius\":0.15,\"fontSizeRatio\":0.06,\"axisWidth\":1,\"zIndex\":0,\"colors\":{\"p"
                "ositive\":\"#5EA2FF\",\"negative\":\"#FF6B6B\",\"track\":null,\"axis\":\"#FFFFFF\",\"l"
                "abel\":\"#FFFFFF\"},\"axisOpacity\":0.5,\"labelOpacity\":0.72}");
/** Bar colors; absent track leaves undrawn cells invisible. */
struct PixelBarChartColors {
  std::string positive = "#5EA2FF", negative = "#FF6B6B", axis = "#FFFFFF", label = "#FFFFFF";
  std::optional<std::string> track;
};
/** Typed inputs consumed directly by the bar chart layout and drawing algorithm. */
struct PixelBarChartOptions : ComponentOptions {
  std::optional<double> min, max;
  int rows = 10, bar_width = 1, bar_spacing = 1;
  double gap = 0.16, corner_radius = 0.15, axis_width = 1;
  std::optional<int> decimals;
  std::optional<bool> show_category_labels;
  bool show_value_labels = true, show_axes = true;
  std::vector<std::string> categories;
  PixelBarChartColors colors;
};
/** Draw bars, axes and labels within a point-sized canvas. */
inline Json pixel_bar_chart(const std::vector<double> &values, const PixelBarChartOptions &o) {
  using namespace component_detail;
  if (values.empty())
    throw GlanceError("pixelBarChart needs values");
  positive(o.rows, "rows");
  positive(o.bar_width, "barWidth");
  options_detail::integer(o.bar_spacing, "barSpacing", 0, 4096);
  double low = 0, high = 0;
  for (const auto &v : values) {
    if (!std::isfinite(v))
      throw GlanceError("pixelBarChart values must be finite");
    low = std::min(low, v);
    high = std::max(high, v);
  }
  low = o.min.value_or(low);
  high = o.max.value_or(high);
  if (!o.min && !o.max && low == high)
    high = low + 1;
  range(low, high);
  double zero = std::clamp(0.0, low, high);
  auto c = canvas(o);
  auto id = identity(o, "pixelBarChart");
  double bw = o.bar_width, spacing = o.bar_spacing;
  double z = o.z_index.value_or(0);
  auto colors = o.colors;
  auto categories = o.categories;
  bool show_categories = o.show_category_labels.value_or(!categories.empty());
  bool show_values = o.show_value_labels, show_axes = o.show_axes;
  if (show_categories && categories.size() != values.size())
    throw GlanceError("categories need one entry per value");
  int decimals = options_detail::integer(
      o.decimals.value_or(std::floor(low) == low && std::floor(high) == high ? 0 : 1), "decimals", 0, 100);
  double ratio = o.font_size_ratio.value_or(0.06), pt = font_pt(ratio, c), line_height = pt * 1.2, label_gap = pt * 0.4;
  double axis_width = o.axis_width, rows = o.rows, gap = o.gap;
  auto box = area(o, c);
  auto a = box.to_pt();
  double half_gap = show_axes || show_values || show_categories ? (gap * a.height) / rows / 2 : 0;
  std::vector<std::pair<double, std::string>> ticks;
  for (double n : {high, zero, low}) {
    if (std::none_of(ticks.begin(), ticks.end(), [&](auto &tick) { return tick.first == n; }))
      ticks.emplace_back(n, format(n, decimals));
  }
  double label_width = 0;
  for (auto &tick : ticks)
    label_width = std::max(label_width, text_width(tick.second, pt));
  double half_label = show_values ? (line_height * 1.5) / 2 : 0,
         category_row = show_categories ? label_gap + text_height(pt) : 0;
  auto plot = box.inset_pt(
      {(show_values ? label_width + label_gap : 0) + (show_axes || show_values ? axis_width + half_gap : 0),
       half_label + half_gap, 0, std::max(half_label, category_row) + half_gap});
  auto rect = emitted(plot, c);
  double columns = static_cast<double>(values.size()) * bw + static_cast<double>(values.size() - 1) * spacing;
  SolveGridOptions solve;
  solve.region = rect;
  solve.columns = columns;
  solve.rows = rows;
  solve.gap = gap;
  auto grid = solve_grid(solve);
  auto layout = grid.layout;
  auto g = fit_matrix(rect, GridSize{columns, rows}, layout);
  double baseline = round((rows * (high - zero)) / (high - low));
  MatrixOptions mo;
  mo.layout = layout;
  mo.base.opacity = 1;
  mo.base.corner_radius = o.corner_radius;
  if (colors.track.has_value())
    mo.default_style = normalize_color(*colors.track);
  Matrix m(columns, rows, mo);
  for (std::size_t i = 0; i < values.size(); ++i) {
    double n = std::clamp(values[i], low, high), x = static_cast<double>(i) * (bw + spacing);
    if (n > zero) {
      double cells = round(((n - zero) / (high - zero)) * baseline);
      m.rect(x, baseline - cells, bw, cells, colors.positive);
    } else if (n < zero) {
      double cells = round(((zero - n) / (zero - low)) * (rows - baseline));
      m.rect(x, baseline, bw, cells, colors.negative);
    }
  }
  auto placed = place_part(m.to_part(), id.first + ":plot", id.second, plot, z);
  detail::Args layers{placed};
  double axis_x = std::min(column_boundary(g, 0), g.origin_x - axis_width / 2);
  if (show_axes) {
    AxisLineOptions axis;
    axis.z_index = z + 1;
    axis.style.color = colors.axis;
    axis.style.stroke_width = axis_width;
    axis.style.opacity = 0.5;
    axis.id = id.first + ":axis-value";
    axis.at = axis_x / c.width;
    axis.from = row_boundary(g, 0) / c.height;
    axis.to = row_boundary(g, rows) / c.height;
    layers.push_back(vertical_line(axis));
    axis.id = id.first + ":axis-base";
    axis.at = row_boundary(g, baseline) / c.height;
    axis.from = (axis_x - axis_width / 2) / c.width;
    axis.to = column_boundary(g, columns) / c.width;
    layers.push_back(horizontal_line(axis));
  }
  if (show_values) {
    double h = line_height * 1.5, right = axis_x - axis_width / 2 - label_gap;
    for (std::size_t i = 0; i < ticks.size(); ++i) {
      double boundary = ticks[i].first == high  ? 0
                        : ticks[i].first == low ? rows
                                                : baseline,
             cy = row_boundary(g, boundary);
      layers.push_back(label(id.first + ":value-" + std::to_string(i),
                             Box::from_pt({a.x, cy - h / 2, std::max(0.0, right - a.x), h}, c), ticks[i].second,
                             "right", "center", ratio, colors.label, 0.72, z + 1));
    }
  }
  if (show_categories) {
    double pitch = (bw + spacing) * (g.cell_width + g.gap_x);
    for (std::size_t i = 0; i < categories.size(); ++i) {
      double start = static_cast<double>(i) * (bw + spacing),
             cx = (column_boundary(g, start) + column_boundary(g, start + bw)) / 2;
      double w = std::min({pitch, 2 * (cx - a.x), 2 * (a.x + a.width - cx)});
      layers.push_back(label(id.first + ":category-" + std::to_string(i),
                             Box::from_pt({cx - w / 2, row_boundary(g, rows) + label_gap, w, text_height(pt)}, c),
                             categories[i], "center", "top", ratio, colors.label, 0.72, z + 1));
    }
  }
  return composite(layers);
}
/** Parse JSON once at the fixture/protocol boundary. */
inline Json pixel_bar_chart(const Json &values, const Json &input) {
  using namespace component_detail;
  PixelBarChartOptions o;
  static_cast<ComponentOptions &>(o) = common_options(input);
  o.min = geometry_detail::number(input, "min");
  o.max = geometry_detail::number(input, "max");
  o.rows = positive(number(input, "rows", 10), "rows");
  o.bar_width = positive(number(input, "barWidth", 1), "barWidth");
  o.bar_spacing = options_detail::integer(number(input, "barSpacing", 1), "barSpacing", 0, 4096);
  o.gap = number(input, "gap", 0.16);
  o.corner_radius = number(input, "cornerRadius", 0.15);
  o.axis_width = number(input, "axisWidth", 1);
  if (present(input, "decimals"))
    o.decimals = options_detail::integer(number(input, "decimals"), "decimals", 0, 100);
  if (present(input, "showCategoryLabels"))
    o.show_category_labels = boolean(input, "showCategoryLabels");
  o.show_value_labels = boolean(input, "showValueLabels", true);
  o.show_axes = boolean(input, "showAxes", true);
  if (present(input, "categories"))
    for (const auto &item : detail::iter(input["categories"]))
      o.categories.push_back(item.string());
  const auto &colors = input["colors"];
  o.colors.positive = string(colors, "positive", o.colors.positive);
  o.colors.negative = string(colors, "negative", o.colors.negative);
  o.colors.axis = string(colors, "axis", o.colors.axis);
  o.colors.label = string(colors, "label", o.colors.label);
  o.colors.track = layers_detail::optional_string(colors, "track");
  std::vector<double> data;
  for (const auto &item : detail::iter(values))
    data.push_back(item.number());
  return pixel_bar_chart(data, o);
}
} // namespace glance_widgets
