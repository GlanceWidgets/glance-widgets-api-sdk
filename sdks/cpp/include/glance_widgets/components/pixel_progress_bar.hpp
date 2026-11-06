#pragma once
#include "component_options.hpp"
namespace glance_widgets {
inline const Json PIXEL_PROGRESS_BAR_DEFAULTS =
    Json::parse("{\"min\":0,\"max\":100,\"orientation\":\"horizontal\",\"segments\":10,\"segmentSize"
                "\":3,\"segmentGap\":1,\"thickness\":2}");
inline const Json LABELED_PIXEL_PROGRESS_BAR_DEFAULTS =
    Json::parse(R"({"id":"progress","labelPosition":"end","fontSizeRatio":0.07,"labelOpacity":1,"zIndex":0})");
struct PixelProgressBarOptions : ComponentOptions {
  double min = 0, max = 100;
  int segments = 10, segment_size = 3, segment_gap = 1;
  std::optional<double> thickness;
  bool vertical = false, reverse = false;
  std::optional<Rect> region;
  std::optional<PixelInput> on, off;
};
/** Render a segmented progress bar using native pixel styles. */
inline Json pixel_progress_bar(double value, const PixelProgressBarOptions &o) {
  using namespace component_detail;
  range(o.min, o.max);
  positive(o.segments, "segments");
  positive(o.segment_size, "segmentSize");
  options_detail::integer(o.segment_gap, "segmentGap", 0, 4096);
  if (o.thickness)
    positive(*o.thickness, "thickness");
  double length = double(o.segments) * o.segment_size + double(o.segments - 1) * o.segment_gap;
  auto target = o.region ? o.region : region(static_cast<const ComponentOptions &>(o));
  auto across = o.thickness ? o.thickness : target ? std::optional<double>{} : std::optional<double>{2};
  auto p = plan(target, o.vertical ? across : length, o.vertical ? length : across, o.theme);
  auto m = matrix(p, o.theme);
  double lit = round(fraction(value, o.min, o.max) * o.segments);
  for (int i = 0; i < o.segments; ++i) {
    int slot = o.vertical != o.reverse ? o.segments - 1 - i : i;
    double offset = double(slot) * (o.segment_size + o.segment_gap);
    auto style =
        i < lit ? o.on.value_or(PixelInput{o.theme.palette.on}) : o.off.value_or(PixelInput{o.theme.default_style});
    if (o.vertical)
      m.rect(0, offset, p.columns, o.segment_size, style);
    else
      m.rect(offset, 0, o.segment_size, p.rows, style);
  }
  return m.to_part();
}
inline PixelProgressBarOptions pixel_progress_options(const Json &o) {
  using namespace component_detail;
  PixelProgressBarOptions p;
  static_cast<ComponentOptions &>(p) = common_options(o);
  p.min = number(o, "min", 0);
  p.max = number(o, "max", 100);
  p.segments = positive(number(o, "segments", 10), "segments");
  p.segment_size = positive(number(o, "segmentSize", 3), "segmentSize");
  p.segment_gap = options_detail::integer(number(o, "segmentGap", 1), "segmentGap", 0, 4096);
  p.thickness = geometry_detail::number(o, "thickness");
  p.vertical = string(o, "orientation", "horizontal") == "vertical";
  p.reverse = boolean(o, "reverse");
  p.theme = theme(o);
  p.region = region(o);
  auto look = looks(p.theme, value(o, "colors", Json::object()));
  p.on = look.first;
  p.off = look.second;
  return p;
}
inline Json pixel_progress_bar(double n, const Json &o = Json::object()) { return pixel_progress_bar(n, pixel_progress_options(o)); }
struct LabeledPixelProgressBarOptions : PixelProgressBarOptions {
  std::optional<std::string> label, label_color;
  std::string label_position = "end";
};
/** Fit a progress bar and its value label into the supplied frame. */
inline Json labeled_pixel_progress_bar(double n, const LabeledPixelProgressBarOptions &o) {
  using namespace component_detail;
  auto c = canvas(o);
  auto id = identity(o, "progress");
  auto t = o.theme;
  double z = o.z_index.value_or(0);
  double min = o.min, max = o.max;
  range(min, max);
  bool vertical = o.vertical, end = o.label_position == "end";
  auto content = o.label.value_or(percent_label(fraction(n, min, max)));
  auto box = area(o, c);
  auto r = box.to_pt();
  double line_box = label_lines();
  double across = vertical ? fit_font(content, r.width) : r.height / line_box;
  double along = vertical ? r.height / (line_box * 3) : fit_font(content, r.width / 3);
  double pt = font_pt(font_ratio(std::min({font_pt(o.font_size_ratio.value_or(0.07), c), across, along}), c), c);
  double slot = vertical ? pt * line_box : text_width(content, pt), reserved = slot + pt * 0.4;
  Insets inset;
  if (vertical)
    (end ? inset.bottom : inset.top) = reserved;
  else
    (end ? inset.right : inset.left) = reserved;
  auto bar = box.inset_pt(inset);
  Rect lr = vertical ? Rect{r.x, end ? (r.y + r.height) - slot : r.y, r.width, slot}
                     : Rect{end ? (r.x + r.width) - slot : r.x, r.y, slot, r.height};
  PixelProgressBarOptions po = o;
  po.region = emitted(bar, c);
  auto placed = place_part(pixel_progress_bar(n, po), id.first + ":bar", id.second, bar, z);
  detail::Args layers{placed};
  layers.push_back(label(id.first + ":label", Box::from_pt(lr, c), content,
                         vertical ? "center"
                         : end    ? "right"
                                  : "left",
                         "center", font_ratio(pt, c), o.label_color.value_or(t.palette.on), 1, z + 1));
  return composite(layers);
}
inline Json labeled_pixel_progress_bar(double n, const Json &input) {
  LabeledPixelProgressBarOptions o;
  static_cast<PixelProgressBarOptions &>(o) = pixel_progress_options(input);
  o.label = layers_detail::optional_string(input, "label");
  o.label_color = layers_detail::optional_string(input["colors"], "label");
  o.label_position = options_detail::string(input, "labelPosition", "end");
  return labeled_pixel_progress_bar(n, o);
}
} // namespace glance_widgets
