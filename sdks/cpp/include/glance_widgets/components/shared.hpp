#pragma once
#include "../generated/advances.hpp"
#include "../layers.hpp"
#include "../matrix.hpp"
#include "../themes.hpp"
namespace glance_widgets {
inline constexpr const char *DEFAULT_THEME_NAME = "lcd";
inline const Json STATUS_COLORS =
    Json::parse(R"({"good":"#34C759","warn":"#FF9F0A","bad":"#FF453A","neutral":"#8E8E93"})");
struct RegionOptions {
  std::optional<Canvas> canvas;
  std::optional<Frame> frame;
};
struct PixelColors {
  std::optional<std::string> on, off, label;
  bool empty_off = false;
};
namespace component_detail {
using options_detail::boolean;
using options_detail::number;
using options_detail::present;
using options_detail::string;
using options_detail::value;
inline double round(double n) { return std::floor(n + 0.5); }
inline void range(double min, double max) {
  if (!std::isfinite(min) || !std::isfinite(max) || !(max > min))
    throw GlanceError("max must be greater than min");
}
inline double fraction(double n, double min, double max) {
  if (!std::isfinite(n))
    throw GlanceError("value must be finite");
  return max > min ? std::clamp((n - min) / (max - min), 0.0, 1.0) : 0;
}
inline int positive(double n, const std::string &what) { return options_detail::integer(n, what, 1, 4096); }
inline PixelTheme theme(const Json &o) {
  const auto input = value(o, "theme", "lcd");
  if (input.is_string())
    return get_theme(input.string());
  PixelTheme t;
  t.name = string(input, "name");
  t.default_style = pixel_detail::resolve(pixel_detail::input(input["default"]));
  t.base = pixel_detail::resolve(pixel_detail::input(input["base"]));
  t.layout = geometry_detail::layout(input["layout"]);
  const auto &p = input["palette"];
  t.palette = {string(p, "on"), string(p, "off"), string(p, "dim"), string(p, "accent")};
  return t;
}
inline Canvas canvas(const Json &o) {
  if (!present(o, "canvas"))
    throw GlanceError("Component needs canvas size in points");
  return require_canvas(geometry_detail::canvas(o["canvas"]), "component");
}
inline Box area(const Json &o, Canvas c) {
  if (!present(o, "frame"))
    return Box::canvas(c);
  return Box::from_frame(geometry_detail::frame(o["frame"]), c);
}
inline Rect emitted(const Box &b, Canvas c) { return resolve_frame(b.frame(), c); }
inline std::optional<Rect> region(const Json &o) {
  if (!present(o, "canvas")) {
    if (present(o, "frame"))
      throw GlanceError("A frame needs canvas size");
    return {};
  }
  auto c = canvas(o);
  return emitted(area(o, c), c);
}
struct Plan {
  double columns, rows;
  MatrixLayout layout;
};
inline Plan plan(std::optional<Rect> region, std::optional<double> columns, std::optional<double> rows,
                 const PixelTheme &t) {
  if (!region) {
    if (!columns || !rows)
      throw GlanceError("Missing grid dimensions");
    return {*columns, *rows, t.layout};
  }
  SolveGridOptions o;
  o.region = *region;
  o.columns = columns;
  o.rows = rows;
  o.gap_mode = t.layout.gap_mode;
  o.clamp_aspect = true;
  auto solved = solve_grid(o);
  return {solved.columns, solved.rows, solved.layout};
}
inline Matrix matrix(const Plan &p, const PixelTheme &t, PixelInput def = EMPTY) {
  MatrixOptions o;
  o.default_style = def;
  o.base = pixel_overrides(t.base);
  o.layout = p.layout;
  return Matrix(p.columns, p.rows, o);
}
inline PixelColors pixel_colors(const Json &colors) {
  PixelColors result;
  result.on = layers_detail::optional_string(colors, "on");
  result.off = layers_detail::optional_string(colors, "off");
  result.label = layers_detail::optional_string(colors, "label");
  result.empty_off = detail::has(colors, "off") && colors["off"].is_null();
  return result;
}
inline std::pair<PixelInput, PixelInput> looks(const PixelTheme &t, const PixelColors &colors) {
  PixelInput on = colors.on.value_or(t.palette.on), off = t.default_style;
  if (colors.empty_off)
    off = EMPTY;
  else if (colors.off && !colors.off->empty()) {
    auto p = t.default_style;
    p.color = *colors.off;
    off = p;
  }
  return {on, off};
}
inline std::pair<PixelInput, PixelInput> looks(const PixelTheme &t, const Json &colors) {
  return looks(t, pixel_colors(colors));
}
inline std::pair<std::string, std::string> identity(const Json &o, const std::string &fallback) {
  auto id = string(o, "id", fallback);
  if (!present(o, "path") && !path_detail::is_identifier(id))
    throw GlanceError("id is not an identifier; supply path");
  return {id, string(o, "path", "data." + id)};
}
inline double advance(const std::string &content) {
  double total = 0;
  for (const auto &ch : detail::chars(content)) {
    unsigned c = static_cast<unsigned char>(ch[0]);
    auto it = std::find_if(std::begin(generated::extra_advances), std::end(generated::extra_advances),
                           [&](const auto &item) { return ch == item.first; });
    total += (ch.size() == 1 && c >= 32 && c < 127        ? generated::ascii_advances[c - 32]
              : it == std::end(generated::extra_advances) ? 1000
                                                          : it->second) +
             40;
  }
  return total / 1000;
}
inline double text_width(const std::string &s, double pt) { return advance(s) * 1.05 * pt + 1; }
inline double fit_font(const std::string &s, double width) {
  return geometry_detail::js_max(0.0, width - 1) / geometry_detail::js_max(1e-9, advance(s) * 1.05);
}
inline double font_pt(double ratio, Canvas c) { return ratio * std::min(c.width, c.height); }
inline double font_ratio(double pt, Canvas c) { return std::clamp(pt / std::min(c.width, c.height), 0.02, 0.28); }
inline std::string percent_label(double fraction) { return detail::numstr(round(fraction * 100)) + "%"; }
inline double text_height(double pt, double lines = 1) { return lines * 1.2 * pt + 1; }
inline double label_lines() {
  volatile double line = 1.2;
  return line * 1.5;
}
inline Json label(const std::string &id, const Box &box, const std::string &content, const std::string &horizontal,
                  const std::string &vertical, double ratio, const std::string &color, double opacity, double z,
                  const std::optional<std::string> &weight = {}) {
  TextOptions o;
  o.id = id;
  o.frame = box.frame();
  o.text = content;
  o.z_index = z;
  o.style.color = color;
  o.style.text_opacity = opacity;
  o.style.font_size_ratio = ratio;
  o.style.horizontal_alignment = horizontal;
  o.style.vertical_alignment = vertical;
  o.style.font_weight = weight;
  return text(o);
}
inline Json composite(detail::Args layers) {
  return output_detail::combine(layers);
}
inline Json place_part(const Json &part, const std::string &id, const std::string &path, const Box &box, double z) {
  return place(part, PlaceOptions{id, box.frame(), path, z, {}, {}});
}
inline std::string format(double n, int decimals) {
  auto s = detail::fixed(n, decimals);
  // Number(formatted) === 0 without locale-dependent parsing: only '-', '0' and '.' and at least one digit.
  const bool zero = s.find('0') != std::string::npos && s.find_first_not_of("-0.") == std::string::npos;
  return zero ? detail::fixed(0, decimals) : s;
}
} // namespace component_detail
} // namespace glance_widgets
