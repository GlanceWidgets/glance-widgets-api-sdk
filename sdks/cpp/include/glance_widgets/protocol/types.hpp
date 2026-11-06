#pragma once
#include "../pixel.hpp"
namespace glance_widgets {
/** Optional renderer style fields shared by native layer builders. */
struct LayerStyle {
  std::optional<std::string> color, background_color, border_color, mask_color, readability_gradient, font_weight,
      font_family, horizontal_alignment, vertical_alignment, pattern, cap, icon_glow_color, line_cap, line_join,
      fill_color, effect_type;
  std::optional<double> opacity, corner_radius_ratio, border_opacity, border_width, gradient_angle, mask_opacity,
      vignette_intensity, text_opacity, font_size_ratio, letter_spacing, baseline_ratio, padding_ratio, stroke_width,
      dash_length, dash_gap, icon_glow_opacity, icon_glow_radius, fill_opacity, plot_padding;
  std::optional<bool> gradient_enabled, auto_wrap, icon_glow_enabled;
  std::optional<std::vector<std::string>> gradient_colors, font_family_fallback;
  std::optional<std::vector<double>> gradient_stops;
  Json to_json() const {
    Json out = Json::object();
    if (color)
      out["color"] = Json(*color);
    if (background_color)
      out["backgroundColor"] = Json(*background_color);
    if (border_color)
      out["borderColor"] = Json(*border_color);
    if (mask_color)
      out["maskColor"] = Json(*mask_color);
    if (readability_gradient)
      out["readabilityGradient"] = Json(*readability_gradient);
    if (font_weight)
      out["fontWeight"] = Json(*font_weight);
    if (font_family)
      out["fontFamily"] = Json(*font_family);
    if (horizontal_alignment)
      out["horizontalAlignment"] = Json(*horizontal_alignment);
    if (vertical_alignment)
      out["verticalAlignment"] = Json(*vertical_alignment);
    if (pattern)
      out["pattern"] = Json(*pattern);
    if (cap)
      out["cap"] = Json(*cap);
    if (icon_glow_color)
      out["iconGlowColor"] = Json(*icon_glow_color);
    if (line_cap)
      out["lineCap"] = Json(*line_cap);
    if (line_join)
      out["lineJoin"] = Json(*line_join);
    if (fill_color)
      out["fillColor"] = Json(*fill_color);
    if (effect_type)
      out["effectType"] = Json(*effect_type);
    if (opacity)
      out["opacity"] = Json(*opacity);
    if (corner_radius_ratio)
      out["cornerRadiusRatio"] = Json(*corner_radius_ratio);
    if (border_opacity)
      out["borderOpacity"] = Json(*border_opacity);
    if (border_width)
      out["borderWidth"] = Json(*border_width);
    if (gradient_angle)
      out["gradientAngle"] = Json(*gradient_angle);
    if (mask_opacity)
      out["maskOpacity"] = Json(*mask_opacity);
    if (vignette_intensity)
      out["vignetteIntensity"] = Json(*vignette_intensity);
    if (text_opacity)
      out["textOpacity"] = Json(*text_opacity);
    if (font_size_ratio)
      out["fontSizeRatio"] = Json(*font_size_ratio);
    if (letter_spacing)
      out["letterSpacing"] = Json(*letter_spacing);
    if (baseline_ratio)
      out["baselineRatio"] = Json(*baseline_ratio);
    if (padding_ratio)
      out["paddingRatio"] = Json(*padding_ratio);
    if (stroke_width)
      out["strokeWidth"] = Json(*stroke_width);
    if (dash_length)
      out["dashLength"] = Json(*dash_length);
    if (dash_gap)
      out["dashGap"] = Json(*dash_gap);
    if (icon_glow_opacity)
      out["iconGlowOpacity"] = Json(*icon_glow_opacity);
    if (icon_glow_radius)
      out["iconGlowRadius"] = Json(*icon_glow_radius);
    if (fill_opacity)
      out["fillOpacity"] = Json(*fill_opacity);
    if (plot_padding)
      out["plotPadding"] = Json(*plot_padding);
    if (gradient_enabled)
      out["gradientEnabled"] = Json(*gradient_enabled);
    if (auto_wrap)
      out["autoWrap"] = Json(*auto_wrap);
    if (icon_glow_enabled)
      out["iconGlowEnabled"] = Json(*icon_glow_enabled);
    if (gradient_colors)
      out["gradientColors"] = Json(*gradient_colors);
    if (font_family_fallback)
      out["fontFamilyFallback"] = Json(*font_family_fallback);
    if (gradient_stops)
      out["gradientStops"] = Json(*gradient_stops);
    return out;
  }
};
using TextStyle = LayerStyle;
using LineStyle = LayerStyle;
using ShapeStyle = LayerStyle;
using ImageStyle = LayerStyle;
using IconStyle = LayerStyle;
using CurveStyle = LayerStyle;
namespace layers_detail {
inline LayerStyle style(const Json &input) {
  LayerStyle result;
  if (options_detail::present(input, "color"))
    result.color = input["color"].string();
  if (options_detail::present(input, "backgroundColor"))
    result.background_color = input["backgroundColor"].string();
  if (options_detail::present(input, "borderColor"))
    result.border_color = input["borderColor"].string();
  if (options_detail::present(input, "maskColor"))
    result.mask_color = input["maskColor"].string();
  if (options_detail::present(input, "readabilityGradient"))
    result.readability_gradient = input["readabilityGradient"].string();
  if (options_detail::present(input, "fontWeight"))
    result.font_weight = input["fontWeight"].string();
  if (options_detail::present(input, "fontFamily"))
    result.font_family = input["fontFamily"].string();
  if (options_detail::present(input, "horizontalAlignment"))
    result.horizontal_alignment = input["horizontalAlignment"].string();
  if (options_detail::present(input, "verticalAlignment"))
    result.vertical_alignment = input["verticalAlignment"].string();
  if (options_detail::present(input, "pattern"))
    result.pattern = input["pattern"].string();
  if (options_detail::present(input, "cap"))
    result.cap = input["cap"].string();
  if (options_detail::present(input, "iconGlowColor"))
    result.icon_glow_color = input["iconGlowColor"].string();
  if (options_detail::present(input, "lineCap"))
    result.line_cap = input["lineCap"].string();
  if (options_detail::present(input, "lineJoin"))
    result.line_join = input["lineJoin"].string();
  if (options_detail::present(input, "fillColor"))
    result.fill_color = input["fillColor"].string();
  if (options_detail::present(input, "effectType"))
    result.effect_type = input["effectType"].string();
  if (options_detail::present(input, "opacity"))
    result.opacity = input["opacity"].number();
  if (options_detail::present(input, "cornerRadiusRatio"))
    result.corner_radius_ratio = input["cornerRadiusRatio"].number();
  if (options_detail::present(input, "borderOpacity"))
    result.border_opacity = input["borderOpacity"].number();
  if (options_detail::present(input, "borderWidth"))
    result.border_width = input["borderWidth"].number();
  if (options_detail::present(input, "gradientAngle"))
    result.gradient_angle = input["gradientAngle"].number();
  if (options_detail::present(input, "maskOpacity"))
    result.mask_opacity = input["maskOpacity"].number();
  if (options_detail::present(input, "vignetteIntensity"))
    result.vignette_intensity = input["vignetteIntensity"].number();
  if (options_detail::present(input, "textOpacity"))
    result.text_opacity = input["textOpacity"].number();
  if (options_detail::present(input, "fontSizeRatio"))
    result.font_size_ratio = input["fontSizeRatio"].number();
  if (options_detail::present(input, "letterSpacing"))
    result.letter_spacing = input["letterSpacing"].number();
  if (options_detail::present(input, "baselineRatio"))
    result.baseline_ratio = input["baselineRatio"].number();
  if (options_detail::present(input, "paddingRatio"))
    result.padding_ratio = input["paddingRatio"].number();
  if (options_detail::present(input, "strokeWidth"))
    result.stroke_width = input["strokeWidth"].number();
  if (options_detail::present(input, "dashLength"))
    result.dash_length = input["dashLength"].number();
  if (options_detail::present(input, "dashGap"))
    result.dash_gap = input["dashGap"].number();
  if (options_detail::present(input, "iconGlowOpacity"))
    result.icon_glow_opacity = input["iconGlowOpacity"].number();
  if (options_detail::present(input, "iconGlowRadius"))
    result.icon_glow_radius = input["iconGlowRadius"].number();
  if (options_detail::present(input, "fillOpacity"))
    result.fill_opacity = input["fillOpacity"].number();
  if (options_detail::present(input, "plotPadding"))
    result.plot_padding = input["plotPadding"].number();
  if (options_detail::present(input, "gradientEnabled"))
    result.gradient_enabled = input["gradientEnabled"].boolean();
  if (options_detail::present(input, "autoWrap"))
    result.auto_wrap = input["autoWrap"].boolean();
  if (options_detail::present(input, "iconGlowEnabled"))
    result.icon_glow_enabled = input["iconGlowEnabled"].boolean();
  if (options_detail::present(input, "gradientColors")) {
    result.gradient_colors.emplace();
    for (const auto &v : detail::iter(input["gradientColors"]))
      result.gradient_colors->push_back(v.string());
  }
  if (options_detail::present(input, "fontFamilyFallback")) {
    result.font_family_fallback.emplace();
    for (const auto &v : detail::iter(input["fontFamilyFallback"]))
      result.font_family_fallback->push_back(v.string());
  }
  if (options_detail::present(input, "gradientStops")) {
    result.gradient_stops.emplace();
    for (const auto &v : detail::iter(input["gradientStops"]))
      result.gradient_stops->push_back(v.number());
  }
  return result;
}
} // namespace layers_detail
} // namespace glance_widgets
