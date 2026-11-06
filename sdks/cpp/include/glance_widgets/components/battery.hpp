#pragma once
#include "modern.hpp"
namespace glance_widgets {
struct BatteryColors {
  /** One color for the level bar; default green / amber / red by level. */
  std::optional<std::string> fill, good, warn, bad, track, label;
};
struct BatteryOptions : ComponentOptions {
  double min = 0, max = 100;
  /** Body height in points, 5-12. Default 12. */
  double height = 12;
  /** Body width in points. Default 2.3 x height, within the area. */
  std::optional<double> width;
  bool show_label = false;
  std::optional<std::string> label;
  BatteryColors colors;
};
/** At or below this fraction a battery is `bad`, up to WARN_LEVEL it is `warn`. */
inline constexpr double BAD_LEVEL = 0.2, WARN_LEVEL = 0.4;
/** Modern battery: translucent capsule body, vertical capsule tab and a rounded level bar colored by charge. */
inline Json battery(double level, const BatteryOptions &o) {
  using namespace component_detail;
  using namespace modern_detail;
  constexpr double height_min = 5, inset = 2, tab_height_ratio = 0.5, tab_width_ratio = 0.25, tab_gap = 1,
                   width_ratio = 2.3, label_gap_em = 0.4;
  auto c = canvas(o);
  auto id = identity(o, "battery");
  range(o.min, o.max);
  double height = o.height;
  if (!(height >= height_min && height <= CURVE_STROKE_MAX_PT))
    throw GlanceError("battery: height must be between " + detail::numstr(height_min) + " and " +
                      detail::numstr(CURVE_STROKE_MAX_PT) + " pt, got " + detail::numstr(height));
  double z = o.z_index.value_or(0);
  double f = fraction(level, o.min, o.max);
  const auto &colors = o.colors;
  std::string track = colors.track.value_or(TRACK_COLOR);
  std::string level_color =
      colors.fill ? *colors.fill
      : f <= BAD_LEVEL
          ? colors.bad.value_or(STATUS_COLORS["bad"].string())
          : f <= WARN_LEVEL ? colors.warn.value_or(STATUS_COLORS["warn"].string())
                            : colors.good.value_or(STATUS_COLORS["good"].string());
  auto a = area(o, c).to_pt();
  if (a.height < height)
    throw GlanceError("battery: the area must be at least " + detail::numstr(height) + " pt high, got " +
                      detail::numstr(a.height));
  double tab_width = height * tab_width_ratio;
  double middle = a.y + a.height / 2;
  double reserved = 0;
  detail::Args label_layers;
  if (o.show_label) {
    auto content = o.label.value_or(percent_label(f));
    double requested = font_pt(o.font_size_ratio.value_or(0.07), c);
    double pt = font_pt(font_ratio(std::min(requested, height / label_lines()), c), c);
    double slot = text_width(content, pt);
    reserved = slot + pt * label_gap_em;
    label_layers.push_back(label(id.first + ":label",
                                 Box::from_pt({a.x + a.width - slot, a.y, slot, a.height}, c), content, "right",
                                 "center", font_ratio(pt, c), colors.label.value_or(LABEL_COLOR), 1, z + 1));
  }
  double available = a.width - reserved - tab_gap - tab_width;
  double body_width = std::min(available, o.width.value_or(height * width_ratio));
  if (!(body_width >= height + 0.1))
    throw GlanceError("battery: the area is too narrow (body " + detail::numstr(body_width) + " pt)");
  double left = a.x;
  detail::Args layers;
  CurveCapsuleSpec body;
  body.id = id.first + ":body";
  body.canvas = c;
  body.from = left;
  body.to = left + body_width;
  body.at = middle;
  body.width = height;
  body.color = track;
  body.opacity = TRACK_OPACITY;
  body.z_index = z;
  layers.push_back(curve_capsule(body));
  CapsuleSpec tab;
  tab.id = id.first + ":tab";
  tab.canvas = c;
  tab.vertical = true;
  tab.at = left + body_width + tab_gap + tab_width / 2;
  tab.from = middle - (height * tab_height_ratio) / 2;
  tab.to = middle + (height * tab_height_ratio) / 2;
  tab.width = tab_width;
  tab.color = track;
  tab.opacity = TRACK_OPACITY;
  tab.z_index = z;
  layers.push_back(capsule(tab));
  double thickness = height - 2 * inset;
  double length = fill_length(f, body_width - 2 * inset, thickness);
  if (length > 0) {
    CurveCapsuleSpec bar;
    bar.id = id.first + ":level";
    bar.canvas = c;
    bar.from = left + inset;
    bar.to = left + inset + length;
    bar.at = middle;
    bar.width = thickness;
    bar.color = level_color;
    bar.opacity = 1;
    bar.z_index = z + 1;
    layers.push_back(curve_capsule(bar));
  }
  layers.insert(layers.end(), label_layers.begin(), label_layers.end());
  return composite(layers);
}
inline Json battery(double level, const Json &input = Json::object()) {
  using namespace component_detail;
  BatteryOptions o;
  static_cast<ComponentOptions &>(o) = common_options(input);
  o.min = number(input, "min", 0);
  o.max = number(input, "max", 100);
  o.height = number(input, "height", 12);
  o.width = geometry_detail::number(input, "width");
  modern_detail::parse_label(input, o.show_label, o.label);
  const auto &colors = input["colors"];
  for (auto [key, target] : {std::pair<const char *, std::optional<std::string> *>{"fill", &o.colors.fill},
                             {"good", &o.colors.good},
                             {"warn", &o.colors.warn},
                             {"bad", &o.colors.bad},
                             {"track", &o.colors.track},
                             {"label", &o.colors.label}})
    *target = layers_detail::optional_string(colors, key);
  return battery(level, o);
}
} // namespace glance_widgets
