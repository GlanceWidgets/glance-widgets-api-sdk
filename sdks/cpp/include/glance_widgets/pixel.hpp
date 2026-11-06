#pragma once
#include "color.hpp"
#include "geometry.hpp"
#include "options.hpp"
#include <variant>
namespace glance_widgets {
struct PixelStyle {
  std::string color = "#FFFFFF";
  double opacity = 1, corner_radius = 0, rotation = 0, halo = 0;
  Json to_json() const {
    return Json::object({{"color", color},
                         {"opacity", opacity},
                         {"cornerRadius", corner_radius},
                         {"rotation", rotation},
                         {"halo", halo}});
  }
};
struct PixelOverrides {
  std::optional<std::string> color;
  std::optional<double> opacity, corner_radius, rotation, halo;
};
struct Empty {};
inline constexpr Empty EMPTY{};
inline const PixelStyle DEFAULT_PIXEL_BASE{};
inline constexpr int BATCH_MIN_OCCURRENCES = 2;
using PixelInput = std::variant<std::nullptr_t, Empty, std::string, PixelOverrides, PixelStyle>;
struct MatrixOptions {
  PixelInput default_style = EMPTY;
  PixelOverrides base;
  MatrixLayout layout;
};
struct FillOptions {
  bool fill = true;
};
struct ArcOptions {
  int thickness = 1;
};
struct FloodFillOptions {
  bool diagonal = false;
};
inline PixelOverrides pixel_overrides(const PixelStyle &style) {
  PixelOverrides out;
  out.color = style.color;
  out.opacity = style.opacity;
  out.corner_radius = style.corner_radius;
  out.rotation = style.rotation;
  out.halo = style.halo;
  return out;
}
namespace pixel_detail {
inline PixelOverrides overrides(const Json &o) {
  if (!o.is_object() && !o.is_null())
    throw GlanceError("Pixel style must be an object");
  for (const auto *key : {"color", "opacity", "cornerRadius", "rotation", "halo"})
    if (detail::has(o, key) && o[key].is_null())
      throw GlanceError(std::string("Pixel ") + key + " cannot be null");
  PixelOverrides p;
  if (options_detail::present(o, "color"))
    p.color = options_detail::string(o, "color");
  p.opacity = geometry_detail::number(o, "opacity");
  p.corner_radius = geometry_detail::number(o, "cornerRadius");
  p.rotation = geometry_detail::number(o, "rotation");
  p.halo = geometry_detail::number(o, "halo");
  return p;
}
inline PixelInput input(const Json &value) {
  if (value.is_null())
    return nullptr;
  if (value.is_string())
    return value.string() == "empty" ? PixelInput(EMPTY) : PixelInput(value.string());
  return overrides(value);
}
inline PixelStyle resolve(const PixelInput &input, PixelStyle base = {}) {
  if (const auto *style = std::get_if<PixelStyle>(&input))
    base = *style;
  if (const auto *color = std::get_if<std::string>(&input))
    base.color = *color;
  if (const auto *p = std::get_if<PixelOverrides>(&input)) {
    if (p->color)
      base.color = *p->color;
    if (p->opacity)
      base.opacity = *p->opacity;
    if (p->corner_radius)
      base.corner_radius = *p->corner_radius;
    if (p->rotation)
      base.rotation = *p->rotation;
    if (p->halo)
      base.halo = *p->halo;
  }
  for (double n : {base.opacity, base.corner_radius, base.rotation, base.halo})
    if (!std::isfinite(n))
      throw GlanceError("Pixel fields must be finite");
  base.color = normalize_color(base.color);
  base.opacity = std::clamp(base.opacity, 0.0, 1.0);
  base.corner_radius = std::clamp(base.corner_radius, 0.0, 0.5);
  base.halo = std::clamp(base.halo, 0.0, 1.0);
  return base;
}
inline MatrixOptions options(const Json &o) {
  MatrixOptions m;
  if (options_detail::present(o, "default"))
    m.default_style = input(o["default"]);
  m.base = overrides(options_detail::value(o, "base"));
  m.layout = geometry_detail::layout(options_detail::value(o, "layout", Json::object()));
  return m;
}
inline Json layout_json(const MatrixLayout &layout) {
  if (!geometry_detail::gap_ratio(layout.gap_mode))
    throw GlanceError("Unknown gapMode");
  Json result = Json::object({{"gapMode", layout.gap_mode},
                              {"edgeInsetRatio", layout.edge_inset_ratio},
                              {"cellAspectRatio", layout.cell_aspect_ratio}});
  if (layout.column_gap_ratio)
    result["columnGapRatio"] = *layout.column_gap_ratio;
  if (layout.row_gap_ratio)
    result["rowGapRatio"] = *layout.row_gap_ratio;
  for (const auto &f : result.p->fields)
    if (f.first != "gapMode" && !std::isfinite(f.second.number()))
      throw GlanceError("Layout fields must be finite");
  return result;
}
} // namespace pixel_detail
} // namespace glance_widgets
