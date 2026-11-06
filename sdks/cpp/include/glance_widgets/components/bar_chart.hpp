#pragma once
#include "modern.hpp"
#include "pixel_bar_chart.hpp"
namespace glance_widgets {
struct BarChartColors {
  /** Bars at or above the baseline / below it. */
  std::string positive = modern_detail::FILL_COLOR, negative = "#FF453A", baseline = "#FFFFFF",
              label = modern_detail::LABEL_COLOR;
};
struct BarChartOptions : ComponentOptions {
  std::optional<double> min, max, bar_width;
  std::vector<std::string> categories;
  bool show_baseline = true;
  BarChartColors colors;
};
/** Modern bar chart: one round-capped line per bar, a faint zero baseline and optional category labels. */
inline Json bar_chart(const std::vector<double> &values, const BarChartOptions &o) {
  using namespace component_detail;
  using namespace modern_detail;
  if (values.empty())
    throw GlanceError("barChart: needs at least one value");
  double zero_low = 0, zero_high = 0;
  for (double v : values) {
    if (!std::isfinite(v))
      throw GlanceError("barChart: values must be finite");
    zero_low = std::min(zero_low, v);
    zero_high = std::max(zero_high, v);
  }
  auto c = canvas(o);
  auto id = identity(o, "barChart");
  double z = o.z_index.value_or(0);
  const auto &categories = o.categories;
  if (!categories.empty() && categories.size() != values.size())
    throw GlanceError("barChart: categories must have one entry per value");
  double low = o.min.value_or(zero_low), high = o.max.value_or(zero_high);
  if (!o.min && !o.max && low == high)
    high = low + 1;
  if (!(high > low))
    throw GlanceError("barChart: max (" + detail::numstr(high) + ") must be greater than min (" +
                      detail::numstr(low) + ")");
  double ratio = o.font_size_ratio.value_or(0.06);
  double pt = ratio * std::min(c.width, c.height);
  auto a = area(o, c).to_pt();
  double label_row = categories.empty() ? 0 : pt * 0.4 + text_height(pt);
  double plot_height = a.height - label_row;
  double slot = a.width / double(values.size());
  double bar_width = o.bar_width.value_or(std::min(LINE_STROKE_MAX_PT, slot * 0.5));
  require_stroke("barChart", "barWidth", bar_width, LINE_STROKE_MAX_PT);
  double minimum = min_capsule_length(c, true, bar_width);
  if (!(plot_height > minimum))
    throw GlanceError("barChart: the area is too short");
  auto y_of = [&](double v) { return a.y + (1 - fraction(v, low, high)) * plot_height; };
  double zero = std::min(high, std::max(low, 0.0));
  double base_y = y_of(zero);
  detail::Args layers;
  for (std::size_t index = 0; index < values.size(); ++index) {
    double clamped = std::min(high, std::max(low, values[index]));
    if (clamped == zero)
      continue;
    double value_y = y_of(clamped);
    bool positive_bar = clamped > zero;
    CapsuleSpec spec;
    spec.id = id.first + ":bar-" + std::to_string(index);
    spec.canvas = c;
    spec.vertical = true;
    spec.at = a.x + (double(index) + 0.5) * slot;
    spec.from = positive_bar ? std::min(value_y, base_y - minimum) : base_y;
    spec.to = positive_bar ? base_y : std::max(value_y, base_y + minimum);
    spec.width = bar_width;
    spec.color = positive_bar ? o.colors.positive : o.colors.negative;
    spec.z_index = z;
    layers.push_back(capsule(spec));
  }
  if (o.show_baseline) {
    AxisLineOptions axis;
    axis.id = id.first + ":baseline";
    axis.at = base_y / c.height;
    axis.from = a.x / c.width;
    axis.to = (a.x + a.width) / c.width;
    axis.z_index = z;
    axis.style.color = o.colors.baseline;
    axis.style.stroke_width = 1;
    axis.style.opacity = 0.3;
    layers.push_back(horizontal_line(axis));
  }
  for (std::size_t index = 0; index < categories.size(); ++index)
    layers.push_back(label(
        id.first + ":category-" + std::to_string(index),
        Box::from_pt({a.x + double(index) * slot, a.y + plot_height + pt * 0.4, slot, text_height(pt)}, c),
        categories[index], "center", "top", ratio, o.colors.label, 0.72, z));
  return composite(layers);
}
inline Json bar_chart(const Json &values, const Json &input) {
  using namespace component_detail;
  BarChartOptions o;
  static_cast<ComponentOptions &>(o) = common_options(input);
  o.min = geometry_detail::number(input, "min");
  o.max = geometry_detail::number(input, "max");
  o.bar_width = geometry_detail::number(input, "barWidth");
  if (present(input, "categories"))
    for (const auto &item : detail::iter(input["categories"]))
      o.categories.push_back(item.string());
  o.show_baseline = boolean(input, "showBaseline", true);
  const auto &colors = input["colors"];
  o.colors.positive = string(colors, "positive", o.colors.positive);
  o.colors.negative = string(colors, "negative", o.colors.negative);
  o.colors.baseline = string(colors, "baseline", o.colors.baseline);
  o.colors.label = string(colors, "label", o.colors.label);
  std::vector<double> data;
  for (const auto &item : detail::iter(values))
    data.push_back(item.number());
  return bar_chart(data, o);
}
} // namespace glance_widgets
