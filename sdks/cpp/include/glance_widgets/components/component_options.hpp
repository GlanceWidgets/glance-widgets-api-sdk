#pragma once
#include "shared.hpp"
namespace glance_widgets {
/** Shared placement and labeling settings for component calls. */
struct ComponentOptions {
  std::optional<Canvas> canvas;
  std::optional<Frame> frame;
  std::optional<std::string> id, path;
  std::optional<double> z_index, font_size_ratio;
  PixelTheme theme = themes.lcd;
};

namespace component_detail {
inline ComponentOptions common_options(const Json &o) {
  ComponentOptions result;
  if (present(o, "canvas"))
    result.canvas = canvas(o);
  if (present(o, "frame")) {
    const auto &f = o["frame"];
    result.frame = Frame{geometry_detail::number(f, "left"),  geometry_detail::number(f, "right"),
                         geometry_detail::number(f, "top"),   geometry_detail::number(f, "bottom"),
                         geometry_detail::number(f, "width"), geometry_detail::number(f, "height")};
  }
  result.id = layers_detail::optional_string(o, "id");
  result.path = layers_detail::optional_string(o, "path");
  result.z_index = geometry_detail::number(o, "zIndex");
  result.font_size_ratio = geometry_detail::number(o, "fontSizeRatio");
  result.theme = theme(o);
  return result;
}
inline Canvas canvas(const ComponentOptions &o) {
  if (!o.canvas)
    throw GlanceError("Component needs canvas size in points");
  return require_canvas(*o.canvas, "component");
}
inline Box area(const ComponentOptions &o, Canvas c) { return o.frame ? Box::from_frame(*o.frame, c) : Box::canvas(c); }
inline std::optional<Rect> region(const ComponentOptions &o) {
  if (!o.canvas) {
    if (o.frame)
      throw GlanceError("A frame needs canvas size");
    return {};
  }
  auto c = canvas(o);
  return emitted(area(o, c), c);
}
inline std::string color(const std::map<std::string, std::string> &colors, const std::string &key,
                         const std::string &fallback) {
  auto it = colors.find(key);
  return it == colors.end() ? fallback : it->second;
}
inline std::map<std::string, std::string> color_options(const Json &input) {
  std::map<std::string, std::string> result;
  if (!input.is_null())
    for (const auto &field : input.p->fields)
      if (!field.second.is_null())
        result[field.first] = field.second.string();
  return result;
}
inline std::pair<std::string, std::string> identity(const ComponentOptions &o, const std::string &fallback) {
  auto id = o.id.value_or(fallback);
  if (!o.path && !path_detail::is_identifier(id))
    throw GlanceError("id is not an identifier; supply path");
  return {id, o.path.value_or("data." + id)};
}
} // namespace component_detail
} // namespace glance_widgets
