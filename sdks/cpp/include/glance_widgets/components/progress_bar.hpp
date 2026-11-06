#pragma once
#include "modern.hpp"
#include "pixel_progress_bar.hpp"
namespace glance_widgets {
struct ProgressBarColors {
  std::string fill = modern_detail::FILL_COLOR, track = modern_detail::TRACK_COLOR, label = modern_detail::LABEL_COLOR;
};
struct ProgressBarOptions : ComponentOptions {
  double min = 0, max = 100, thickness = 6;
  bool vertical = false, reverse = false, show_label = false, label_at_end = true;
  std::optional<std::string> label;
  ProgressBarColors colors;
};
/** Modern progress bar: a rounded track and fill (2 layers, 3 with a label); canvas required. */
inline Json progress_bar(double value, const ProgressBarOptions &o) {
  using namespace component_detail;
  using namespace modern_detail;
  auto c = canvas(o);
  auto id = identity(o, "progress");
  range(o.min, o.max);
  require_stroke("progressBar", "thickness", o.thickness, LINE_STROKE_MAX_PT);
  bool vertical = o.vertical;
  double z = o.z_index.value_or(0);
  double f = fraction(value, o.min, o.max);
  auto a = area(o, c).to_pt();
  double from = vertical ? a.y : a.x, to = vertical ? a.y + a.height : a.x + a.width;
  detail::Args labels;
  if (o.show_label) {
    auto content = o.label.value_or(percent_label(f));
    bool at_end = o.label_at_end;
    double line_box = label_lines();
    double across = vertical ? fit_font(content, a.width) : a.height / line_box;
    double along = vertical ? a.height / (line_box * 3) : fit_font(content, a.width / 3);
    double requested = font_pt(o.font_size_ratio.value_or(0.07), c);
    double pt = font_pt(font_ratio(std::min({requested, across, along}), c), c);
    double slot = vertical ? pt * line_box : text_width(content, pt);
    double reserved = slot + pt * 0.4;
    Rect rect = vertical ? Rect{a.x, at_end ? to - slot : from, a.width, slot}
                         : Rect{at_end ? to - slot : from, a.y, slot, a.height};
    if (at_end)
      to -= reserved;
    else
      from += reserved;
    labels.push_back(label(id.first + ":label", Box::from_pt(rect, c), content,
                           vertical ? "center"
                           : at_end ? "right"
                                    : "left",
                           "center", font_ratio(pt, c), o.colors.label, 1, z + 1));
  }
  double at = vertical ? a.x + a.width / 2 : a.y + a.height / 2;
  double minimum = min_capsule_length(c, vertical, o.thickness);
  if (!(to - from >= minimum))
    throw GlanceError("progressBar: the area is too short for a " + detail::numstr(o.thickness) + " pt bar");
  CapsuleSpec spec;
  spec.canvas = c;
  spec.vertical = vertical;
  spec.at = at;
  spec.width = o.thickness;
  detail::Args layers;
  spec.id = id.first + ":track";
  spec.from = from;
  spec.to = to;
  spec.color = o.colors.track;
  spec.opacity = TRACK_OPACITY;
  spec.z_index = z;
  layers.push_back(capsule(spec));
  double length = fill_length(f, to - from, minimum);
  if (length > 0) {
    bool from_start = vertical == o.reverse;
    spec.id = id.first + ":fill";
    spec.from = from_start ? from : to - length;
    spec.to = from_start ? from + length : to;
    spec.color = o.colors.fill;
    spec.opacity = 1;
    spec.z_index = z + 1;
    layers.push_back(capsule(spec));
  }
  layers.insert(layers.end(), labels.begin(), labels.end());
  return composite(layers);
}
inline Json progress_bar(double value, const Json &input = Json::object()) {
  using namespace component_detail;
  ProgressBarOptions o;
  static_cast<ComponentOptions &>(o) = common_options(input);
  o.min = number(input, "min", 0);
  o.max = number(input, "max", 100);
  o.thickness = number(input, "thickness", 6);
  o.vertical = string(input, "orientation", "horizontal") == "vertical";
  o.reverse = boolean(input, "reverse");
  modern_detail::parse_label(input, o.show_label, o.label);
  o.label_at_end = string(input, "labelPosition", "end") == "end";
  const auto &colors = input["colors"];
  o.colors.fill = string(colors, "fill", o.colors.fill);
  o.colors.track = string(colors, "track", o.colors.track);
  o.colors.label = string(colors, "label", o.colors.label);
  return progress_bar(value, o);
}
} // namespace glance_widgets
