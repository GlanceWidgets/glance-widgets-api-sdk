#pragma once
#include "crypto.hpp"
#include "json.hpp"
#include <chrono>
#include <optional>

namespace glance_widgets {
inline constexpr const char *TIMESTAMP_HEADER = "X-GlanceWidgets-Timestamp";
inline constexpr const char *SIGNATURE_HEADER = "X-GlanceWidgets-Signature";
inline constexpr int DEFAULT_SIGNATURE_TOLERANCE_MS = 300000;

/** Request headers sent by the App. */
struct SignedRequest {
  std::optional<std::string> timestamp;
  std::optional<std::string> signature;
};
/** Clock and allowed skew, in milliseconds. */
struct VerifyOptions {
  std::optional<double> now;
  std::optional<double> tolerance_ms;
};
namespace signature_detail {
inline std::string text(const Json &value) {
  if (value.is_null())
    return "null";
  if (value.is_string())
    return value.string();
  if (value.is_number())
    return detail::numstr(value.number());
  if (value.is_bool())
    return value.dump();
  if (value.is_array()) {
    std::string result;
    for (std::size_t i = 0; i < value.size(); ++i) {
      if (i)
        result += ',';
      const auto &item = value.p->items[i];
      if (!item.is_null())
        result += text(item);
    }
    return result;
  }
  return "[object Object]";
}
} // namespace signature_detail
/** Compute Base64 HMAC-SHA256 using the App's timestamp/newline/secret message. */
inline std::string sign(const Json &timestamp, const Json &secret) {
  if (!secret.is_string())
    throw GlanceError("$.secret: expected string");
  return detail::signature(signature_detail::text(timestamp), secret.string());
}
/** Check header syntax, freshness and signature using a constant-time comparison. */
inline bool verify_signature(const SignedRequest &request, const std::string &secret,
                             const VerifyOptions &options = {}) {
  if (!request.timestamp || !request.signature || request.timestamp->empty())
    return false;
  for (char c : *request.timestamp)
    if (c < '0' || c > '9')
      return false;
  const double timestamp = std::strtod(request.timestamp->c_str(), nullptr);
  const double now = options.now.value_or(
      std::chrono::duration<double, std::milli>(std::chrono::system_clock::now().time_since_epoch()).count());
  if (std::abs(now - timestamp) > options.tolerance_ms.value_or(DEFAULT_SIGNATURE_TOLERANCE_MS))
    return false;
  return detail::constant_equal(detail::signature(*request.timestamp, secret), *request.signature);
}
/** JSON adapter for protocol fixtures; null options use defaults. */
inline bool verify_signature(const Json &request, const Json &secret, const Json &options = Json::object()) {
  if (!detail::has(request, "timestamp") || !detail::has(request, "signature"))
    return false;
  const auto &timestamp = request["timestamp"];
  const auto &signature = request["signature"];
  if (!timestamp.is_string() || !signature.is_string())
    return false;
  if (!secret.is_string())
    throw GlanceError("$.secret: expected string");
  VerifyOptions parsed;
  for (const auto &key : {"now", "toleranceMs"}) {
    if (options.is_null() || !detail::has(options, key))
      continue;
    const auto &value = options[key];
    if (value.is_null())
      continue;
    if (!value.is_number())
      throw GlanceError(std::string("$.") + key + ": expected number");
    (std::string(key) == "now" ? parsed.now : parsed.tolerance_ms) = value.number();
  }
  return verify_signature(SignedRequest{timestamp.string(), signature.string()}, secret.string(), parsed);
}
} // namespace glance_widgets
