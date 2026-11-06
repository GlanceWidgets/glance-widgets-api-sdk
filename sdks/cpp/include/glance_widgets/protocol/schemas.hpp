#pragma once
#include "../options.hpp"
namespace glance_widgets {
namespace validation_detail {
inline Json array_rule(Json rule, int length = -1) {
  auto out = Json::object({{"arrayOf", rule}});
  if (length >= 0)
    out["length"] = length;
  return out;
}
inline Json enum_rule(std::initializer_list<const char *> values) {
  detail::Args items;
  for (const auto *v : values)
    items.emplace_back(v);
  return Json::object({{"enum", Json::array(items)}});
}
inline Json object_rule(Json fields) { return Json::object({{"object", fields}}); }
inline const Json range_rule = array_rule("number", 2);
inline const Json gap_modes = enum_rule({"zero", "near", "default", "far", "extraFar"});
inline const Json pixel_schema = Json::object({{"color", "color"},
                                               {"opacity", "number"},
                                               {"cornerRadius", "number"},
                                               {"rotation", "number"},
                                               {"halo", "number"},
                                               {"empty", "boolean"}});
inline const Json layout_schema = Json::object({{"gapMode", gap_modes},
                                                {"edgeInsetRatio", "number"},
                                                {"cellAspectRatio", "number"},
                                                {"columnGapRatio", "number"},
                                                {"rowGapRatio", "number"}});
inline const Json frame_schema = Json::object({{"left", "number"},
                                               {"top", "number"},
                                               {"right", "number"},
                                               {"bottom", "number"},
                                               {"width", "number"},
                                               {"height", "number"}});
inline const Json chrome_schema = Json::object(
    {{"effectType", enum_rule({"none", "mask", "vignette", "readability"})},
     {"backgroundColor", "color"},
     {"opacity", "number"},
     {"cornerRadiusRatio", "number"},
     {"borderColor", "color"},
     {"borderOpacity", "number"},
     {"borderWidth", "number"},
     {"gradientEnabled", "boolean"},
     {"gradientColors", array_rule("color")},
     {"gradientStops", array_rule("number")},
     {"gradientAngle", "number"},
     {"maskColor", "color"},
     {"maskOpacity", "number"},
     {"vignetteIntensity", "number"},
     {"readabilityGradient",
      enum_rule({"none", "top", "bottom", "left", "right", "both", "topLeftDiagonal", "topRightDiagonal"})}});
inline const Json marker_schema = Json::object(
    {{"at", object_rule(Json::object({{"x", "number"}, {"sampleIndex", "integer"}, {"offsetPt", range_rule}}))},
     {"symbol", object_rule(Json::object({{"radiusPt", "number"}, {"fill", "color"}}))},
     {"label", "string"},
     {"labelStyle", object_rule(Json::object({{"color", "color"},
                                              {"fontSizePt", "number"},
                                              {"position", enum_rule({"top", "bottom", "left", "right"})}}))}});
inline Json layer_schema(Json content, Json style) { return Json::object({{"content", content}, {"style", style}}); }
inline const Json layer_schemas = Json::object(
    {{"text",
      layer_schema(
          Json::object({{"text", "textValue"}}),
          options_detail::overlay(chrome_schema,
                                  Json::object({{"color", "color"},
                                                {"textOpacity", "number"},
                                                {"fontSizeRatio", "number"},
                                                {"fontWeight", enum_rule({"300", "400", "500", "600", "700"})},
                                                {"fontFamily", enum_rule({"system", "systemMonospace", "monospace"})},
                                                {"fontFamilyFallback", array_rule("string")},
                                                {"letterSpacing", "number"},
                                                {"baselineRatio", "number"},
                                                {"horizontalAlignment", enum_rule({"left", "center", "right"})},
                                                {"verticalAlignment", enum_rule({"top", "center", "bottom"})},
                                                {"autoWrap", "boolean"},
                                                {"paddingRatio", "number"}})))},
     {"line", layer_schema(Json::object({{"direction", enum_rule({"horizontal", "vertical"})}}),
                           Json::object({{"color", "color"},
                                         {"opacity", "number"},
                                         {"strokeWidth", "number"},
                                         {"pattern", enum_rule({"solid", "dashed", "dotted"})},
                                         {"cap", enum_rule({"round", "butt"})},
                                         {"dashLength", "number"},
                                         {"dashGap", "number"},
                                         {"gradientEnabled", "boolean"},
                                         {"gradientColors", array_rule("color")},
                                         {"gradientStops", array_rule("number")}}))},
     {"shape",
      layer_schema(Json::object(), options_detail::overlay(chrome_schema, Json::object({{"color", "color"}})))},
     {"image",
      layer_schema(
          Json::object({{"url", "string"}}),
          options_detail::overlay(chrome_schema, Json::object({{"color", "color"}})))},
     {"icon",
      layer_schema(Json::object({{"icon", "string"},
                                 {"iconCodePoint", "integer"},
                                 {"iconFont", enum_rule({"material", "phosphorBold", "devicon", "simpleIcons"})},
                                 {"url", "string"}}),
                   Json::object({{"color", "color"},
                                 {"textOpacity", "number"},
                                 {"iconGlowEnabled", "boolean"},
                                 {"iconGlowColor", "color"},
                                 {"iconGlowOpacity", "number"},
                                 {"iconGlowRadius", "number"}}))},
     {"matrix", layer_schema(Json::object({{"pixels", "list"}, {"default", "object"}, {"batch", "list"}, {"layout", "object"}}), Json::object())},
     {"curve", layer_schema(Json::object({{"mode", enum_rule({"samples", "function"})},
                                          {"points", array_rule(range_rule)},
                                          {"expression", "string"},
                                          {"parameters", Json::object({{"record", "number"}})},
                                          {"domain", range_rule},
                                          {"yDomain", range_rule},
                                          {"interpolation", enum_rule({"linear", "stepAfter", "monotone"})},
                                          {"markers", array_rule(object_rule(marker_schema))}}),
                            Json::object({{"color", "color"},
                                          {"opacity", "number"},
                                          {"strokeWidth", "number"},
                                          {"lineCap", enum_rule({"round", "butt", "square"})},
                                          {"lineJoin", enum_rule({"round", "bevel", "miter"})},
                                          {"dashLength", "number"},
                                          {"dashGap", "number"},
                                          {"fillColor", "color"},
                                          {"fillOpacity", "number"},
                                          {"plotPadding", "number"}}))}});
} // namespace validation_detail
} // namespace glance_widgets
