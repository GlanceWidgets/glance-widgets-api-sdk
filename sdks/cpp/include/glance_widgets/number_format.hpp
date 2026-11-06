#pragma once
#include "error.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
namespace glance_widgets {
namespace detail {
inline std::string numstr(double n) {
  if (n == 0)
    return "0";
  if (std::isnan(n))
    return "NaN";
  if (!std::isfinite(n))
    return n < 0 ? "-Infinity" : "Infinity";
  const bool negative = n < 0;
  n = std::abs(n);
  // Shortest round-trip digits, independent of the C locale's decimal point.
  std::string digits;
  int point = 0;
  for (int precision = 1; precision <= 17; ++precision) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*e", precision - 1, n);
    if (precision < 17 && std::strtod(buffer, nullptr) != n)
      continue;
    const char *e = std::strchr(buffer, 'e');
    if (e == nullptr)
      throw GlanceError("Number formatting failed");
    digits.clear();
    for (const char *c = buffer; c < e; ++c)
      if (*c >= '0' && *c <= '9')
        digits.push_back(*c);
    point = static_cast<int>(std::strtol(e + 1, nullptr, 10)) + 1;
    break;
  }
  std::string result;
  if (n >= 1e21 || n < 1e-6) {
    while (digits.size() > 1 && digits.back() == '0')
      digits.pop_back();
    result = digits.substr(0, 1);
    if (digits.size() > 1)
      result += "." + digits.substr(1);
    int exp = point - 1;
    result += "e" + std::string(exp >= 0 ? "+" : "") + std::to_string(exp);
  } else if (point <= 0)
    result = "0." + std::string(std::size_t(-point), '0') + digits;
  else if (point >= int(digits.size()))
    result = digits + std::string(std::size_t(point - int(digits.size())), '0');
  else {
    result = digits;
    result.insert(std::size_t(point), 1, '.');
  }
  return (negative ? "-" : "") + result;
}
inline std::string fixed(double value, int decimals) {
  if (decimals < 0 || decimals > 100)
    throw GlanceError("decimals must be 0..100");
  if (!std::isfinite(value) || std::abs(value) >= 1e21)
    return numstr(value);
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::fixed << std::setprecision(1100) << std::abs(value);
  std::string s = stream.str();
  auto point = s.find('.');
  std::string digits = s.substr(0, point) + s.substr(point + 1, static_cast<std::size_t>(decimals));
  if (s.at(point + 1 + static_cast<std::size_t>(decimals)) >= '5') {
    int i = int(digits.size()) - 1;
    for (; i >= 0 && digits[static_cast<std::size_t>(i)] == '9'; i--)
      digits[static_cast<std::size_t>(i)] = '0';
    if (i < 0)
      digits = "1" + digits;
    else
      digits[static_cast<std::size_t>(i)]++;
  }
  if (decimals)
    digits.insert(digits.size() - static_cast<std::size_t>(decimals), 1, '.');
  return (value < 0 ? "-" : "") + digits;
}
} // namespace detail
} // namespace glance_widgets
