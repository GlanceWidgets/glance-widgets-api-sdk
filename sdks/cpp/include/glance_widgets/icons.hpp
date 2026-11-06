#pragma once
#include "generated/icons.hpp"
#include "options.hpp"
namespace glance_widgets {
inline constexpr const char *DEFAULT_ICON_NAME = "ph_activity";
struct IconGlyph {
  std::string icon;
  int icon_code_point;
  std::string icon_font;
  Json to_json() const {
    return Json::object({{"icon", icon}, {"iconCodePoint", icon_code_point}, {"iconFont", icon_font}});
  }
};
namespace icons_detail {
inline const Json &catalog() {
  static const auto data = Json::parse(generated::icons);
  return data;
}
} // namespace icons_detail
inline bool has_icon(const std::string &name) { return detail::has(icons_detail::catalog(), name); }
inline std::vector<std::string> icon_names() {
  std::vector<std::string> out;
  for (const auto &field : icons_detail::catalog().p->fields)
    out.push_back(field.first);
  return out;
}
inline IconGlyph icon_glyph(const std::string &name) {
  if (!has_icon(name))
    throw GlanceError("Unknown icon: " + name);
  const auto &entry = icons_detail::catalog()[name];
  return {name, options_detail::integer(entry["iconCodePoint"].number(), "iconCodePoint", 0, 0x10ffff),
          entry["iconFont"].string()};
}
} // namespace glance_widgets
