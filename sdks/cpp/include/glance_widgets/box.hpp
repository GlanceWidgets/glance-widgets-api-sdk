#pragma once
#include "geometry.hpp"
#include "options.hpp"
namespace glance_widgets {
inline constexpr int FRAME_DECIMALS = 6;
/** Insets in local ratios, or points when used with inset_pt. */
struct Insets {
  double left = 0, top = 0, right = 0, bottom = 0;
  Insets() = default;
  Insets(double all) : left(all), top(all), right(all), bottom(all) {}
  Insets(double l, double t, double r, double b) : left(l), top(t), right(r), bottom(b) {}
  explicit Insets(const Json &value) {
    if (value.is_number()) {
      left = top = right = bottom = value.number();
      return;
    }
    left = options_detail::number(value, "left");
    top = options_detail::number(value, "top");
    right = options_detail::number(value, "right");
    bottom = options_detail::number(value, "bottom");
  }
};
struct SplitOptions {
  double gap = 0;
  std::optional<double> gap_pt;
};
/** Immutable rectangle in canvas ratios. */
class Box {
  std::optional<Canvas> canvas_;
  Canvas require_size() const {
    if (!canvas_)
      throw GlanceError("Box needs the canvas size in points");
    return require_canvas(*canvas_, "Box");
  }

public:
  double left, top, width, height;
  Box(double l, double t, double w, double h, std::optional<Canvas> canvas = {})
      : canvas_(canvas), left(l), top(t), width(w), height(h) {
    for (double n : {l, t, w, h})
      if (!std::isfinite(n))
        throw GlanceError("Box coordinates must be finite");
    if (w < 0 || h < 0)
      throw GlanceError("Box size must not be negative");
  }
  static Box canvas(std::optional<Canvas> size = {}) { return {0, 0, 1, 1, size}; }
  static Box canvas(const Json &size) {
    return canvas(size.is_null() ? std::optional<Canvas>{} : geometry_detail::canvas(size));
  }
  static Box from_pt(Rect rect, Canvas canvas) {
    auto r = to_ratio_rect(rect, canvas);
    return {r.x, r.y, r.width, r.height, canvas};
  }
  static Box from_frame(const Frame &frame, Canvas canvas) { return from_pt(resolve_frame(frame, canvas), canvas); }
  double right() const { return left + width; }
  double bottom() const { return top + height; }
  bool has_canvas() const { return canvas_.has_value(); }
  std::optional<Canvas> canvas_size() const { return canvas_; }
  Box sub(double l, double t, double w, double h) const {
    return {left + l * width, top + t * height, w * width, h * height, canvas_};
  }
  Box inset(Insets side) const {
    return sub(side.left, side.top, 1 - side.left - side.right, 1 - side.top - side.bottom);
  }
  Box inset_pt(Insets side) const {
    auto r = to_pt();
    return from_pt({r.x + side.left, r.y + side.top, geometry_detail::js_max(0.0, r.width - side.left - side.right),
                    geometry_detail::js_max(0.0, r.height - side.top - side.bottom)},
                   require_size());
  }
  std::vector<Box> split(bool vertical, const std::vector<double> &weights, SplitOptions options = {}) const {
    if (weights.empty())
      throw GlanceError("Box.split needs non-negative weights");
    double total = 0;
    for (double w : weights) {
      if (!(w >= 0) || !std::isfinite(w))
        throw GlanceError("Box.split needs finite non-negative weights");
      total += w;
    }
    if (!(total > 0) || !std::isfinite(total))
      throw GlanceError("Box.split invalid weight total");
    const double extent = vertical ? height : width;
    double gap = options.gap * extent;
    if (options.gap_pt) {
      auto c = require_size();
      gap = *options.gap_pt / (vertical ? c.height : c.width);
    }
    const double available = extent - gap * static_cast<double>(weights.size() - 1);
    if (available < 0)
      throw GlanceError("Box.split: gaps are larger than the box");
    double offset = vertical ? top : left;
    std::vector<Box> parts;
    for (double weight : weights) {
      double size = (available * weight) / total;
      parts.emplace_back(vertical ? left : offset, vertical ? offset : top, vertical ? width : size,
                         vertical ? size : height, canvas_);
      offset += size + gap;
    }
    return parts;
  }
  std::vector<Box> rows(const std::vector<double> &weights, SplitOptions o = {}) const {
    return split(true, weights, o);
  }
  std::vector<Box> columns(const std::vector<double> &weights, SplitOptions o = {}) const {
    return split(false, weights, o);
  }
  std::vector<Box> rows(int count, SplitOptions o = {}) const {
    if (count < 1)
      throw GlanceError("Box.rows needs a positive count");
    return rows(std::vector<double>(static_cast<std::size_t>(count), 1), o);
  }
  std::vector<Box> columns(int count, SplitOptions o = {}) const {
    if (count < 1)
      throw GlanceError("Box.columns needs a positive count");
    return columns(std::vector<double>(static_cast<std::size_t>(count), 1), o);
  }
  Rect to_pt() const {
    auto c = require_size();
    return {left * c.width, top * c.height, width * c.width, height * c.height};
  }
  operator Json() const { return to_frame(); }
  Json to_frame() const { return frame_json(frame()); }
  Frame frame() const {
    auto axis = [](double start, double size) {
      if (size > 1.5)
        throw GlanceError("Frame size exceeds App maximum 1.5");
      if (size < 0.02) {
        start = start + size / 2 - 0.02 / 2;
        size = 0.02;
      }
      constexpr double scale = 1000000;
      return std::pair<double, double>{std::floor(start * scale + 0.5) / scale, std::floor(size * scale + 0.5) / scale};
    };
    auto x = axis(left, width), y = axis(top, height);
    return {x.first, {}, y.first, {}, x.second, y.second};
  }
};
} // namespace glance_widgets
