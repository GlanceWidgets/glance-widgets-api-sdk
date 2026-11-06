#pragma once
#include "modern.hpp"
namespace glance_widgets {
struct SignalColors {
  /** Lit bars (default white); unlit bars default to `fill`, dimmed. */
  std::string fill = "#FFFFFF";
  std::optional<std::string> track;
};
struct SignalOptions : ComponentOptions {
  double min = 0, max = 100;
  int bars = 4;
  /** Height of the tallest bar in points. Default the area height, at most 14. */
  std::optional<double> height;
  /** Bar width in points, 0.5-8. Default 26% of the height. */
  std::optional<double> bar_width;
  SignalColors colors;
};
/** Modern reception indicator: ascending round-capped bars, lit up to the level. */
inline Json signal(double level, const SignalOptions &o) {
  using namespace component_detail;
  using namespace modern_detail;
  auto c = canvas(o);
  auto id = identity(o, "signal");
  range(o.min, o.max);
  positive(o.bars, "bars");
  auto a = area(o, c).to_pt();
  constexpr double max_height = 14, bar_width_ratio = 0.26, gap_ratio = 0.5, shortest_ratio = 0.35;
  double height = o.height.value_or(std::min(max_height, a.height));
  double width = o.bar_width.value_or(height * bar_width_ratio);
  require_stroke("signal", "barWidth", width, LINE_STROKE_MAX_PT);
  double gap = width * gap_ratio;
  double minimum = min_capsule_length(c, true, width);
  if (!(height >= minimum && height <= a.height))
    throw GlanceError("signal: height " + detail::numstr(height) + " pt does not fit the area");
  if (o.bars * width + (o.bars - 1) * gap > a.width)
    throw GlanceError("signal: the area is too narrow");
  const auto &fill = o.colors.fill;
  const auto track = o.colors.track.value_or(fill);
  double lit = round(fraction(level, o.min, o.max) * o.bars);
  double bottom = a.y + (a.height + height) / 2;
  detail::Args layers;
  for (int index = 0; index < o.bars; ++index) {
    double share = o.bars == 1 ? 1 : shortest_ratio + ((1 - shortest_ratio) * index) / (o.bars - 1);
    CapsuleSpec spec;
    spec.id = id.first + ":bar-" + std::to_string(index);
    spec.canvas = c;
    spec.vertical = true;
    spec.at = a.x + width / 2 + index * (width + gap);
    spec.from = bottom - std::max(minimum, height * share);
    spec.to = bottom;
    spec.width = width;
    spec.color = index < lit ? fill : track;
    spec.opacity = index < lit ? 1 : TRACK_OPACITY;
    spec.z_index = o.z_index.value_or(0);
    layers.push_back(capsule(spec));
  }
  return composite(layers);
}
inline Json signal(double level, const Json &input = Json::object()) {
  using namespace component_detail;
  SignalOptions o;
  static_cast<ComponentOptions &>(o) = common_options(input);
  o.min = number(input, "min", 0);
  o.max = number(input, "max", 100);
  o.bars = positive(number(input, "bars", 4), "bars");
  o.height = geometry_detail::number(input, "height");
  o.bar_width = geometry_detail::number(input, "barWidth");
  const auto &colors = input["colors"];
  o.colors.fill = string(colors, "fill", o.colors.fill);
  o.colors.track = layers_detail::optional_string(colors, "track");
  return signal(level, o);
}
} // namespace glance_widgets
