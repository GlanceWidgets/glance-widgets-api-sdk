#pragma once
#include "pixel.hpp"
namespace glance_widgets {
inline constexpr double OFF_PIXEL_OPACITY = 0.3, ROOT_RING_OPACITY_THRESHOLD = 0.4;
struct ThemePalette {
  std::string on, off, dim, accent;
};
struct PixelTheme {
  std::string name;
  PixelStyle default_style, base;
  MatrixLayout layout;
  ThemePalette palette;
};
inline PixelTheme get_theme(const std::string &name) {
  PixelTheme t;
  t.name = name;
  if (name == "led") {
    t.palette = {"#FF4D2E", "#3A1F1B", "#A33A26", "#FFD23F"};
    t.base.corner_radius = 0.5;
    t.base.halo = 0.6;
    t.layout.gap_mode = "far";
  } else if (name == "lcd") {
    t.palette = {"#9BFF6B", "#27402B", "#5E9B4A", "#E6FF8A"};
    t.layout.gap_mode = "near";
  } else if (name == "dot") {
    t.palette = {"#F2F2F7", "#48484A", "#8E8E93", "#0A84FF"};
    t.base.corner_radius = 0.5;
    t.layout.gap_mode = "extraFar";
  } else if (name == "diamond") {
    t.palette = {"#5AC8FA", "#1C3A4A", "#3D7C99", "#FF9F0A"};
    t.base.corner_radius = 0.1;
    t.base.rotation = 45;
    t.layout.gap_mode = "extraFar";
  } else if (name == "soft") {
    t.palette = {"#FFE2B8", "#4A3F38", "#B59A82", "#FF7A59"};
    t.base.corner_radius = 0.3;
    t.base.halo = 0.25;
  } else
    throw GlanceError("Unknown theme: " + name);
  t.base.color = t.palette.on;
  t.default_style = t.base;
  t.default_style.color = t.palette.off;
  t.default_style.opacity = OFF_PIXEL_OPACITY;
  t.default_style.halo = 0;
  return t;
}
inline Json theme_json(const PixelTheme &t) {
  return Json::object(
      {{"name", t.name},
       {"default", t.default_style.to_json()},
       {"base", t.base.to_json()},
       {"layout", pixel_detail::layout_json(t.layout)},
       {"palette",
        Json::object(
            {{"on", t.palette.on}, {"off", t.palette.off}, {"dim", t.palette.dim}, {"accent", t.palette.accent}})}});
}
/** Overrides are optional so absent fields retain the theme's values. */
struct ThemeOverrides {
  std::optional<PixelInput> default_style;
  PixelOverrides base;
  std::optional<MatrixLayout> layout;
};
inline MatrixOptions theme_options(const PixelTheme &theme, const ThemeOverrides &overrides = {}) {
  MatrixOptions result;
  result.default_style = overrides.default_style.value_or(PixelInput{theme.default_style});
  result.base = pixel_overrides(pixel_detail::resolve(PixelInput{overrides.base}, theme.base));
  result.layout = overrides.layout.value_or(theme.layout);
  return result;
}
inline Json theme_options(const PixelTheme &t, const Json &overrides) {
  return Json::object({{"default", options_detail::value(overrides, "default", t.default_style.to_json())},
                       {"base", options_detail::overlay(t.base.to_json(), options_detail::value(overrides, "base"))},
                       {"layout", options_detail::overlay(pixel_detail::layout_json(t.layout),
                                                          options_detail::value(overrides, "layout"))}});
}
inline Json theme_options(const Json &t, const Json &overrides = Json::object()) {
  return Json::object({{"default", options_detail::value(overrides, "default", t["default"])},
                       {"base", options_detail::overlay(t["base"], options_detail::value(overrides, "base"))},
                       {"layout", options_detail::overlay(t["layout"], options_detail::value(overrides, "layout"))}});
}
struct Themes {
  PixelTheme led = get_theme("led"), lcd = get_theme("lcd"), dot = get_theme("dot"), diamond = get_theme("diamond"),
             soft = get_theme("soft");
};
inline const Themes themes;
} // namespace glance_widgets
