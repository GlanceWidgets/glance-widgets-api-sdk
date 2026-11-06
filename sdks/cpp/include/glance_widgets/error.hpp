#pragma once
#include <stdexcept>
#include <string>

namespace glance_widgets {
/// Base exception for SDK input and protocol errors.
class GlanceError : public std::runtime_error {
public:
  explicit GlanceError(const std::string &message) : std::runtime_error(message) {}
};
} // namespace glance_widgets
