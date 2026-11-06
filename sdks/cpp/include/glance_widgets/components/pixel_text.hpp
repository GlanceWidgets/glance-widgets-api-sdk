#pragma once
#include "component_options.hpp"
#include "pixel_fit.hpp"
namespace glance_widgets {
struct PixelTextOptions : ComponentOptions {
  PixelFont font = FONT_5X7;
  std::string align = "center", valign = "center";
  std::optional<std::string> color, line_align;
  std::optional<int> letter_spacing, line_spacing;
  std::optional<double> max_cell_size;
  bool monospace = false;
  bool unlit = true;
};
using PixelTextPanelOptions = PixelTextOptions;
inline const Json PIXEL_TEXT_DEFAULTS =
    Json::parse(R"({"font":"5x7","align":"center","valign":"center","unlit":true})");
namespace pixel_text_detail {
inline Matrix content(const std::string &s, const PixelTextOptions &o) {
  using namespace component_detail;
  if (s.empty())
    throw GlanceError("pixelText must not be empty");
  TextLayoutOptions spacing;
  spacing.monospace = o.monospace;
  spacing.font = resolve_font(o.font);
  spacing.letter_spacing = o.letter_spacing;
  spacing.line_spacing = o.line_spacing.value_or(1);
  auto metrics = layout_text(s, spacing);
  auto m = content_matrix(metrics.width, metrics.height, o.theme);
  DrawTextOptions draw;
  static_cast<TextLayoutOptions &>(draw) = spacing;
  draw.box_width = font_detail::integer(metrics.width, "text width");
  draw.align = o.line_align.value_or(o.align);
  if (o.color.has_value())
    draw.style = *o.color;
  return m.text(s, 0, 0, draw);
}
inline Matrix fit(const std::string &s, Rect r, const PixelTextOptions &o) {
  using namespace component_detail;
  return fit_pixel_content(content(s, o), r, {o.theme, o.align, o.valign, o.unlit, o.max_cell_size});
}
} // namespace pixel_text_detail
/** Fit bitmap text into a matrix part. */
inline Json pixel_text_panel(const std::string &s, const PixelTextOptions &o) {
  auto c = component_detail::canvas(o);
  return pixel_text_detail::fit(s, component_detail::emitted(component_detail::area(o, c), c), o).to_part();
}
/** Fit bitmap text into the root matrix. */
inline Json pixel_text_root(const std::string &s, const PixelTextOptions &o) {
  auto c = component_detail::canvas(o);
  return pixel_text_detail::fit(s, {0, 0, c.width, c.height}, o).to_root();
}
namespace pixel_text_detail {
inline PixelTextOptions options(const Json &input) {
  using namespace component_detail;
  PixelTextOptions o;
  static_cast<ComponentOptions &>(o) = common_options(input);
  o.font = resolve_font(value(input, "font", "5x7"));
  o.align = string(input, "align", "center");
  o.valign = string(input, "valign", "center");
  o.monospace = boolean(input, "monospace", false);
  o.unlit = boolean(input, "unlit", true);
  o.color = layers_detail::optional_string(input, "color");
  o.line_align = layers_detail::optional_string(input, "lineAlign");
  if (present(input, "letterSpacing"))
    o.letter_spacing = font_detail::integer(number(input, "letterSpacing"), "letterSpacing");
  if (present(input, "lineSpacing"))
    o.line_spacing = font_detail::integer(number(input, "lineSpacing"), "lineSpacing");
  o.max_cell_size = geometry_detail::number(input, "maxCellSize");
  return o;
}
} // namespace pixel_text_detail
inline Json pixel_text_panel(const std::string &s, const Json &o) {
  return pixel_text_panel(s, pixel_text_detail::options(o));
}
inline Json pixel_text_root(const std::string &s, const Json &o) {
  return pixel_text_root(s, pixel_text_detail::options(o));
}
} // namespace glance_widgets
