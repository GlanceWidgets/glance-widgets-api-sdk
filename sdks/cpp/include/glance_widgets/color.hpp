#pragma once
#include "error.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>

namespace glance_widgets {
/// Color channels in the inclusive range 0–255 before formatting.
struct Rgba {
  double r = 0;
  double g = 0;
  double b = 0;
  double a = 255;
};
namespace color_detail {
inline int hex_digit(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}
inline bool valid(std::string_view s, bool shorthand) {
  if (!s.empty() && s.front() == '#')
    s.remove_prefix(1);
  if (s.size() != 6 && s.size() != 8 && !(shorthand && s.size() == 3))
    return false;
  for (char c : s)
    if (hex_digit(c) < 0)
      return false;
  return true;
}
inline std::string_view trim(std::string_view s) {
  // ECMAScript TrimString whitespace, encoded as UTF-8.
  constexpr std::string_view whitespace[] = {
      " ",        "\t",       "\r",       "\n",       "\v",       "\f",       u8"\u00a0", u8"\u1680", u8"\u2000",
      u8"\u2001", u8"\u2002", u8"\u2003", u8"\u2004", u8"\u2005", u8"\u2006", u8"\u2007", u8"\u2008", u8"\u2009",
      u8"\u200a", u8"\u2028", u8"\u2029", u8"\u202f", u8"\u205f", u8"\u3000", u8"\ufeff"};
  bool changed = true;
  while (changed && !s.empty()) {
    changed = false;
    for (auto w : whitespace) {
      if (s.size() >= w.size() && s.substr(0, w.size()) == w) {
        s.remove_prefix(w.size());
        changed = true;
      }
      if (s.size() >= w.size() && s.substr(s.size() - w.size()) == w) {
        s.remove_suffix(w.size());
        changed = true;
      }
    }
  }
  return s;
}
/// [+-]?(digits[.digits*]|.digits)([eE][+-]?digits)? or [+-]?Infinity, whole string.
inline bool is_numeric_literal(const std::string &v) {
  std::size_t i = 0;
  const auto digit = [&](std::size_t k) { return k < v.size() && v[k] >= '0' && v[k] <= '9'; };
  if (i < v.size() && (v[i] == '+' || v[i] == '-'))
    ++i;
  if (v.compare(i, std::string::npos, "Infinity") == 0)
    return true;
  std::size_t whole = 0, frac = 0;
  while (digit(i)) {
    ++i;
    ++whole;
  }
  if (i < v.size() && v[i] == '.') {
    ++i;
    while (digit(i)) {
      ++i;
      ++frac;
    }
  }
  if (whole == 0 && frac == 0)
    return false;
  if (i < v.size() && (v[i] == 'e' || v[i] == 'E')) {
    ++i;
    if (i < v.size() && (v[i] == '+' || v[i] == '-'))
      ++i;
    if (!digit(i))
      return false;
    while (digit(i))
      ++i;
  }
  return i == v.size();
}
inline int channel(double v) {
  if (!std::isfinite(v))
    throw GlanceError("Invalid color channel");
  return static_cast<int>(std::clamp(std::floor(v + 0.5), 0.0, 255.0));
}
inline std::string byte(double v) {
  constexpr char digits[] = "0123456789ABCDEF";
  const int n = channel(v);
  return {digits[n / 16], digits[n % 16]};
}
} // namespace color_detail
/// Whether the App accepts the color directly (without normalization).
inline bool is_app_color(std::string_view s) { return color_detail::valid(s, false); }
/// Parse hexadecimal RGB or alpha-first ARGB.
inline Rgba parse_color(std::string_view s) {
  s = color_detail::trim(s);
  if (!color_detail::valid(s, true))
    throw GlanceError("Invalid color: expected #RGB, #RRGGBB or #AARRGGBB");
  if (s.front() == '#')
    s.remove_prefix(1);
  if (s.size() == 3)
    return {double(color_detail::hex_digit(s[0]) * 17), double(color_detail::hex_digit(s[1]) * 17),
            double(color_detail::hex_digit(s[2]) * 17), 255};
  auto byte = [&](std::size_t i) {
    return double(color_detail::hex_digit(s[i]) * 16 + color_detail::hex_digit(s[i + 1]));
  };
  const std::size_t start = s.size() == 8 ? 2 : 0;
  return {byte(start), byte(start + 2), byte(start + 4), start == 2 ? byte(0) : 255};
}
/// Round and clamp channels, omitting fully opaque alpha.
inline std::string format_color(const Rgba &c) {
  const auto channels = color_detail::byte(c.r) + color_detail::byte(c.g) + color_detail::byte(c.b);
  return "#" + (color_detail::channel(c.a) == 255 ? "" : color_detail::byte(c.a)) + channels;
}
/// Normalize any supported hexadecimal color.
inline std::string normalize_color(std::string_view s) { return format_color(parse_color(s)); }
/// Create a color; alpha is expressed from zero to one.
inline std::string rgb(double r, double g, double b, double alpha = 1) { return format_color({r, g, b, alpha * 255}); }
/// Replace alpha, preserving RGB channels.
inline std::string with_alpha(std::string_view s, double alpha) {
  auto c = parse_color(s);
  c.a = alpha * 255;
  return format_color(c);
}
/// Numeric-string alpha follows JavaScript numeric conversion.
inline std::string with_alpha(std::string_view s, const std::string &alpha) {
  auto text = color_detail::trim(alpha);
  if (text.empty())
    return with_alpha(s, 0.0);
  if (text.size() > 2 && text[0] == '0') {
    int base = text[1] == 'x' || text[1] == 'X'   ? 16
               : text[1] == 'b' || text[1] == 'B' ? 2
               : text[1] == 'o' || text[1] == 'O' ? 8
                                                  : 0;
    if (base) {
      double value = 0;
      for (std::size_t i = 2; i < text.size(); ++i) {
        int digit = color_detail::hex_digit(text[i]);
        if (digit < 0 || digit >= base)
          throw GlanceError("Invalid alpha");
        value = value * base + digit;
      }
      return with_alpha(s, value);
    }
  }
  std::string value(text);
  if (!color_detail::is_numeric_literal(value))
    throw GlanceError("Invalid alpha");
  return with_alpha(s, std::strtod(value.c_str(), nullptr));
}
/// Interpolate all four channels with a clamped fraction.
inline std::string mix_colors(std::string_view from, std::string_view to, double t) {
  auto a = parse_color(from), b = parse_color(to);
  const double amount = std::clamp(t, 0.0, 1.0);
  return format_color(
      {a.r + (b.r - a.r) * amount, a.g + (b.g - a.g) * amount, a.b + (b.b - a.b) * amount, a.a + (b.a - a.a) * amount});
}
} // namespace glance_widgets
