#pragma once
#include "json.hpp"
#include <optional>
namespace glance_widgets {
namespace options_detail {
inline bool present(const Json &o, const std::string &key) { return detail::has(o, key) && !o[key].is_null(); }
inline Json value(const Json &o, const std::string &key, Json fallback = {}) {
  return present(o, key) ? o[key] : fallback;
}
inline double number(const Json &o, const std::string &key, double fallback = 0) {
  if (!present(o, key))
    return fallback;
  if (!o[key].is_number())
    throw GlanceError("$." + key + ": expected number");
  return o[key].number();
}
inline std::string string(const Json &o, const std::string &key, std::string fallback = "") {
  if (!present(o, key))
    return fallback;
  if (!o[key].is_string())
    throw GlanceError("$." + key + ": expected string");
  return o[key].string();
}
inline bool boolean(const Json &o, const std::string &key, bool fallback = false) {
  if (!present(o, key))
    return fallback;
  if (!o[key].is_bool())
    throw GlanceError("$." + key + ": expected boolean");
  return o[key].boolean();
}
inline int integer(double n, const std::string &name, int lo, int hi) {
  if (!std::isfinite(n) || n != std::floor(n) || n < lo || n > hi)
    throw GlanceError(name + " must be an integer in range " + std::to_string(lo) + ".." + std::to_string(hi));
  return static_cast<int>(n);
}
inline Json overlay(Json base, const Json &extra) {
  if (extra.is_null())
    return base;
  if (!extra.is_object())
    throw GlanceError("Expected object");
  for (const auto &field : extra.p->fields)
    base[field.first] = field.second;
  return base;
}
inline void erase(Json &object, const std::string &key) {
  auto &fields = object.p->fields;
  fields.erase(std::remove_if(fields.begin(), fields.end(), [&](const auto &f) { return f.first == key; }),
               fields.end());
}
} // namespace options_detail
} // namespace glance_widgets
