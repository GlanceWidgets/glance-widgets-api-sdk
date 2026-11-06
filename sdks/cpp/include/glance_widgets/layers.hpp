#pragma once
#include "box.hpp"
#include "icons.hpp"
#include "output.hpp"
#include "protocol/types.hpp"
namespace glance_widgets {
inline const Json TEXT_DEFAULTS =
    Json::parse("{\"color\":\"#FFFFFF\",\"textOpacity\":1,\"opacity\":0,\"fontSizeRatio\":0.12,\"fon"
                "tWeight\":\"400\",\"fontFamily\":\"system\",\"letterSpacing\":0,\"horizontalAlignm"
                "ent\":\"left\",\"verticalAlignment\":\"top\",\"autoWrap\":false,\"paddingRatio\":0}");
inline const Json LINE_DEFAULTS =
    Json::parse(R"({"color":"#FFFFFF","opacity":1,"strokeWidth":2,"pattern":"solid","cap":"butt"})");
inline const Json SHAPE_DEFAULTS = Json::parse(R"({"color":"#2A63D4","opacity":1,"cornerRadiusRatio":0})");
inline const Json ICON_DEFAULTS = Json::parse(R"({"color":"#FFFFFF","textOpacity":1})");
inline const Json IMAGE_DEFAULTS = Json::parse(R"({"opacity":1,"cornerRadiusRatio":0})");
inline const Json CURVE_DEFAULTS =
    Json::parse("{\"color\":\"#6BA7FF\",\"opacity\":1,\"strokeWidth\":2,\"lineCap\":\"round\",\"lineJo"
                "in\":\"round\",\"fillOpacity\":0,\"plotPadding\":0}");
/** Common strongly typed placement and renderer style fields. */
struct LayerOptions {
  std::string id;
  Frame frame;
  std::optional<double> z_index, rotation;
  LayerStyle style;
  std::optional<std::string> data_path;
  Json transform;
};
struct TextOptions : LayerOptions {
  std::optional<std::string> text;
};
struct LineOptions : LayerOptions {
  std::string direction;
};
struct AxisLineOptions : LayerOptions {
  double at = 0, from = 0, to = 1;
};
struct ImageOptions : LayerOptions {
  std::optional<std::string> url;
};
struct IconOptions : ImageOptions {
  std::optional<std::string> name;
  std::optional<IconGlyph> glyph;
};
struct CurveOptions : LayerOptions {
  Json data = Json::object();
};
namespace layers_detail {
inline LayerOptions common(const Json &o) {
  return {options_detail::string(o, "id"), geometry_detail::frame(o["frame"]), geometry_detail::number(o, "zIndex"),
          geometry_detail::number(o, "rotation"), style(o["style"]),
          options_detail::present(o, "dataPath") ? std::optional<std::string>(options_detail::string(o, "dataPath")) : std::nullopt, o["transform"]};
}
inline std::optional<std::string> optional_string(const Json &o, const std::string &key) {
  return options_detail::present(o, key) ? std::optional<std::string>(options_detail::string(o, key)) : std::nullopt;
}
inline Json base(const std::string &type, const LayerOptions &o) {
  Json node = Json::object({{"frame", frame_json(o.frame)}});
  if (o.z_index) node["zIndex"] = *o.z_index;
  if (o.rotation) node["rotation"] = *o.rotation;
  if (!o.transform.is_null()) node["transform"] = o.transform;
  auto binding = o.data_path.value_or(layer_path(o.id));
  return Json::object({{"layers", Json::array({Json::object({{"id", layer_id(o.id)}, {"type", type}, {"dataPath", binding}})})}, {"data", nest_at_path(binding,node)}});
}

inline Json colors(Json style) {
  for (auto &field : style.p->fields) {
    const auto &key = field.first;
    auto &value = field.second;
    if ((key == "color" || (key.size() >= 5 && key.substr(key.size() - 5) == "Color")) && value.is_string())
      value = normalize_color(value.string());
    else if (key == "gradientColors" && value.is_array())
      for (auto &color : value.p->items)
        color = normalize_color(color.string());
  }
  return style;
}
inline Json finish(Json layer, Json content, Json style) {
  auto path = layer["layers"][0]["dataPath"].string();
  auto node = resolve_data_path(layer["data"],path);
  node["content"] = std::move(content);
  node["style"] = colors(std::move(style));
  layer["data"] = nest_at_path(path,node);
  return layer;
}
} // namespace layers_detail
/** Text content with explicit renderer defaults. */
inline Json text(const TextOptions &o) {
  auto content = Json::object({{"text", o.text.value_or("")}});
  return layers_detail::finish(layers_detail::base("text", o), content,
                               options_detail::overlay(TEXT_DEFAULTS, o.style.to_json()));
}
inline Json text(const Json &o) {
  TextOptions t;
  static_cast<LayerOptions &>(t) = layers_detail::common(o);
  t.text = layers_detail::optional_string(o, "text");
  return text(t);
}
/** Create a line with explicit renderer defaults. */
inline Json line(const LineOptions &o) {
  return layers_detail::finish(layers_detail::base("line", o), Json::object({{"direction", o.direction}}),
                               options_detail::overlay(LINE_DEFAULTS, o.style.to_json()));
}
inline Json line(const Json &o) {
  LineOptions l;
  static_cast<LayerOptions &>(l) = layers_detail::common(o);
  l.direction = options_detail::string(o, "direction");
  return line(l);
}
inline Json axis_line(const AxisLineOptions &o, bool vertical) {
  double length = o.to - o.from;
  if (!(length >= FRAME_SIZE_MIN))
    throw GlanceError("Line needs at least 0.02 of canvas");
  LineOptions l;
  static_cast<LayerOptions &>(l) = o;
  l.frame =
      Box(vertical ? o.at : o.from, vertical ? o.from : o.at, vertical ? 0 : length, vertical ? length : 0).frame();
  l.direction = vertical ? "vertical" : "horizontal";
  return line(l);
}
inline Json horizontal_line(const AxisLineOptions &o) { return axis_line(o, false); }
inline Json vertical_line(const AxisLineOptions &o) { return axis_line(o, true); }
inline AxisLineOptions axis_options(const Json &o) {
  AxisLineOptions a;
  static_cast<LayerOptions &>(a) = layers_detail::common(o);
  a.at = options_detail::number(o, "at");
  a.from = options_detail::number(o, "from");
  a.to = options_detail::number(o, "to");
  return a;
}
inline Json horizontal_line(const Json &o) { return horizontal_line(axis_options(o)); }
inline Json vertical_line(const Json &o) { return vertical_line(axis_options(o)); }
/** Create a filled shape; background color falls back to color. */
inline Json shape(const LayerOptions &o) {
  auto style = options_detail::overlay(SHAPE_DEFAULTS, o.style.to_json());
  if (!options_detail::present(style, "backgroundColor"))
    style["backgroundColor"] = style["color"];
  return layers_detail::finish(layers_detail::base("shape", o), Json::object(), style);
}
/** Shape placed in a Box: when the Box knows its canvas, sizes under 24 pt would be enlarged by the App. */
inline Json shape(const Box &box, LayerOptions o) {
  if (box.has_canvas()) {
    const auto rect = box.to_pt();
    if (rect.width < SHAPE_MIN_SIZE_PT || rect.height < SHAPE_MIN_SIZE_PT)
      throw GlanceError("Shape " + o.id + " is smaller than 24pt and would be enlarged by the App");
  }
  o.frame = box.frame();
  return shape(o);
}
inline Json shape(const Json &o) { return shape(layers_detail::common(o)); }
/** Image artwork with explicit renderer defaults. */
inline Json image(const ImageOptions &o) {
  if (!o.url) throw GlanceError("Image needs url");
  auto content = Json::object();
  if (o.url)
    content["url"] = *o.url;
  return layers_detail::finish(layers_detail::base("image", o), content,
                               options_detail::overlay(IMAGE_DEFAULTS, o.style.to_json()));
}
inline Json image(const Json &o) {
  ImageOptions i;
  static_cast<LayerOptions &>(i) = layers_detail::common(o);
  i.url = layers_detail::optional_string(o, "url");
  return image(i);
}
/** Resolve one named, glyph or URL icon source. */
inline Json icon(const IconOptions &o) {
  int sources =
      int(o.url.has_value()) + int(o.name.has_value()) + int(o.glyph.has_value());
  if (sources != 1)
    throw GlanceError("Icon needs exactly one source");
  auto content = Json::object();
  if (o.name || o.glyph) {
    auto glyph = o.glyph ? *o.glyph : icon_glyph(*o.name);
    content = options_detail::overlay(content, glyph.to_json());
  } else if (o.url)
    content["url"] = *o.url;
  return layers_detail::finish(layers_detail::base("icon", o), content,
                               options_detail::overlay(ICON_DEFAULTS, o.style.to_json()));
}
inline Json icon(const Json &o) {
  IconOptions i;
  static_cast<LayerOptions &>(i) = layers_detail::common(o);
  i.url = layers_detail::optional_string(o, "url");
  i.name = layers_detail::optional_string(o, "name");
  if (options_detail::present(o, "glyph")) {
    const auto &g = o["glyph"];
    i.glyph = IconGlyph{g["icon"].string(),
                        options_detail::integer(g["iconCodePoint"].number(), "icon code point", 0, 0x10ffff),
                        g["iconFont"].string()};
  }
  return icon(i);
}
/** Create a curve from protocol/business data with explicit style defaults. */
inline Json curve(const CurveOptions &o) {
  const auto &d = o.data;
  bool function =
      options_detail::string(d, "mode") == "function" || (!detail::has(d, "mode") && detail::has(d, "expression"));
  if (function && (!detail::has(d, "domain") || !detail::has(d, "yDomain")))
    throw GlanceError("Curve function needs domain and yDomain");
  if (d["points"].size() > 1024 || d["markers"].size() > 32)
    throw GlanceError("Curve data limit exceeded");
  if (options_detail::present(d, "markers"))
    for (const auto &m : detail::iter(d["markers"])) {
      const auto offset = options_detail::value(m["at"], "offsetPt", Json::array({0, 0}));
      for (const auto &value : detail::iter(offset))
        if (std::abs(value.number()) > 64)
          throw GlanceError("Curve marker offset exceeds 64pt");
    }
  auto content = d;
  content["mode"] = options_detail::value(d, "mode", detail::has(d, "expression") ? "function" : "samples");
  content["interpolation"] = options_detail::value(d, "interpolation", "monotone");
  auto style = options_detail::overlay(CURVE_DEFAULTS, o.style.to_json());
  if (!options_detail::present(style, "fillColor"))
    style["fillColor"] = style["color"];
  return layers_detail::finish(layers_detail::base("curve", o), content, style);
}
inline Json curve(const Json &o) {
  CurveOptions c;
  static_cast<LayerOptions &>(c) = layers_detail::common(o);
  c.data = options_detail::value(o, "data", Json::object());
  return curve(c);
}
} // namespace glance_widgets
