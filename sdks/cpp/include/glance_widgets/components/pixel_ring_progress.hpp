#pragma once
#include "pixel_progress_bar.hpp"
namespace glance_widgets {
inline const Json PIXEL_RING_PROGRESS_DEFAULTS =
    Json::parse(R"({"min":0,"max":100,"diameter":33,"thickness":4,"startAngle":0})");
inline const Json LABELED_PIXEL_RING_PROGRESS_DEFAULTS = Json::parse(R"({"id":"ring","labelOpacity":1,"zIndex":0})");
struct PixelRingProgressOptions : ComponentOptions {
  double min = 0, max = 100, start_angle = 0;
  int diameter = 33, thickness = 4;
  bool counter_clockwise = false;
  std::optional<PixelInput> on, off;
};
/** Render an integer-rasterized progress ring. */
inline Json pixel_ring_progress(double n, const PixelRingProgressOptions &o) {
  using namespace component_detail;
  range(o.min, o.max);
  positive(o.diameter, "diameter");
  positive(o.thickness, "thickness");
  if (o.diameter < 3 || o.diameter % 2 == 0)
    throw GlanceError("diameter must be odd and at least 3");
  auto m = matrix({double(o.diameter), double(o.diameter), o.theme.layout}, o.theme);
  int r = (o.diameter - 1) / 2;
  double sweep = fraction(n, o.min, o.max) * 360;
  m.arc(r, r, r, 0, 360, o.off.value_or(PixelInput{o.theme.default_style}), {o.thickness});
  if (sweep > 0)
    m.arc(r, r, r, o.counter_clockwise ? o.start_angle - sweep : o.start_angle,
          o.counter_clockwise ? o.start_angle : o.start_angle + sweep, o.on.value_or(PixelInput{o.theme.palette.on}),
          {o.thickness});
  return m.to_part();
}
inline PixelRingProgressOptions pixel_ring_options(const Json &o) {
  using namespace component_detail;
  PixelRingProgressOptions r;
  static_cast<ComponentOptions &>(r) = common_options(o);
  r.min = number(o, "min");
  r.max = number(o, "max", 100);
  r.start_angle = number(o, "startAngle");
  r.diameter = positive(number(o, "diameter", 33), "diameter");
  r.thickness = positive(number(o, "thickness", 4), "thickness");
  r.counter_clockwise = boolean(o, "counterClockwise");
  r.theme = theme(o);
  auto look = looks(r.theme, value(o, "colors", Json::object()));
  r.on = look.first;
  r.off = look.second;
  return r;
}
inline Json pixel_ring_progress(double n, const Json &o = Json::object()) { return pixel_ring_progress(n, pixel_ring_options(o)); }
struct LabeledPixelRingProgressOptions : PixelRingProgressOptions {
  std::optional<std::string> label, label_color;
  std::string label_position = "end";
};
/** Fit a progress ring with a centered label. */
inline Json labeled_pixel_ring_progress(double n, const LabeledPixelRingProgressOptions &o) {
  using namespace component_detail;
  auto c = canvas(o);
  auto id = identity(o, "ring");
  auto t = o.theme;
  double z = o.z_index.value_or(0);
  auto content = o.label.value_or(percent_label(fraction(n, o.min, o.max)));
  auto r = area(o, c).to_pt();
  double side = std::min(r.width, r.height);
  auto box = Box::from_pt({r.x + (r.width - side) / 2, r.y + (r.height - side) / 2, side, side}, c);
  auto part = pixel_ring_progress(n, o);
  auto placed = place_part(part, id.first + ":ring", id.second, box, z);
  double d = o.diameter;
  auto g = fit_matrix(emitted(box, c), GridSize{d, d}, o.theme.layout);
  auto center = cell_rect(g, (d - 1) / 2, (d - 1) / 2);
  double hole = std::max(1.0, (d - 1) / 2 - o.thickness) * (g.cell_width + g.gap_x);
  double hw = 1.6 * hole, hh = 1.2 * hole;
  double requested = o.font_size_ratio.has_value() ? font_pt(*o.font_size_ratio, c)
                                                   : std::min(hh / label_lines(), fit_font(content, hw));
  double pt = font_pt(font_ratio(requested, c), c), w = std::max(hw, text_width(content, pt)),
         h = std::max(hh, text_height(pt));
  auto label_box = Box::from_pt({center.x + center.width / 2 - w / 2, center.y + center.height / 2 - h / 2, w, h}, c);
  detail::Args layers{placed};
  layers.push_back(label(id.first + ":label", label_box, content, "center", "center", font_ratio(pt, c),
                         o.label_color.value_or(t.palette.on), 1, z + 1));
  return composite(layers);
}
inline Json labeled_pixel_ring_progress(double n, const Json &input) {
  LabeledPixelRingProgressOptions o;
  static_cast<PixelRingProgressOptions &>(o) = pixel_ring_options(input);
  o.label = layers_detail::optional_string(input, "label");
  o.label_color = layers_detail::optional_string(input["colors"], "label");
  o.label_position = options_detail::string(input, "labelPosition", "end");
  return labeled_pixel_ring_progress(n, o);
}
} // namespace glance_widgets
