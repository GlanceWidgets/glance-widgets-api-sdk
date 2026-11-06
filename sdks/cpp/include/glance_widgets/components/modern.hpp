#pragma once
#include "component_options.hpp"
/**
 * Shared pieces of the modern (layer-built, not matrix) components.
 *
 * The App renders shapes at least 24 pt wide and high, so thin or partial pieces are drawn with `line` layers
 * (stroke 0.5-8 pt) and `curve` layers (stroke 0.25-12 pt), both with round caps. A round cap reaches half the stroke
 * width past the frame, so `capsule` insets its frame by that much and its from / to are the visible ends.
 */
namespace glance_widgets {
namespace modern_detail {
/** iOS system blue; track is a neutral gray that reads on light and dark cards. */
inline constexpr const char *FILL_COLOR = "#0A84FF", *TRACK_COLOR = "#8E8E93", *LABEL_COLOR = "#FFFFFF";
inline constexpr double TRACK_OPACITY = 0.3, LINE_STROKE_MAX_PT = 8, CURVE_STROKE_MAX_PT = 12,
                       CURVE_POINT_DECIMALS = 1e3;
inline void require_stroke(const std::string &what, const std::string &name, double value, double max) {
  if (!std::isfinite(value) || value < 0.5 || value > max)
    throw GlanceError(what + ": " + name + " must be between 0.5 and " + detail::numstr(max) + " pt, got " +
                      detail::numstr(value));
}
/** Shortest visible capsule: a frame needs at least FRAME_SIZE_MIN of the canvas along its axis. */
inline double min_capsule_length(Canvas canvas, bool vertical, double width) {
  return width + FRAME_SIZE_MIN * (vertical ? canvas.height : canvas.width);
}
struct CapsuleSpec {
  std::string id;
  Canvas canvas;
  bool vertical = false;
  /** Center across the line (y for horizontal, x for vertical), points. */
  double at = 0;
  /** Visible ends along the line, points. */
  double from = 0, to = 0, width = 0;
  std::string color;
  double opacity = 1, z_index = 0;
};
/** A round-capped line whose visible extent is exactly [from, to]. */
inline Json capsule(const CapsuleSpec &s) {
  const Canvas &canvas = s.canvas;
  double start = s.from + s.width / 2;
  double end = std::max(s.to - s.width / 2, start + FRAME_SIZE_MIN * (s.vertical ? canvas.height : canvas.width));
  LineOptions o;
  o.id = s.id;
  o.frame = (s.vertical ? Box(s.at / canvas.width, start / canvas.height, 0, (end - start) / canvas.height)
                        : Box(start / canvas.width, s.at / canvas.height, (end - start) / canvas.width, 0))
                .frame();
  o.direction = s.vertical ? "vertical" : "horizontal";
  o.z_index = s.z_index;
  o.style.color = s.color;
  o.style.opacity = s.opacity;
  o.style.stroke_width = s.width;
  o.style.cap = "round";
  return line(o);
}
struct CurveCapsuleSpec {
  std::string id;
  Canvas canvas;
  /** Visible horizontal extent, points. */
  double from = 0, to = 0;
  /** Center y, points. */
  double at = 0;
  /** Thickness in points, 0.5-12. */
  double width = 0;
  std::string color;
  double opacity = 1, z_index = 0;
};
/** A horizontal round-capped bar up to 12 pt thick: a 2-point curve whose visible extent is exactly [from, to]. */
inline Json curve_capsule(const CurveCapsuleSpec &s) {
  const Canvas &canvas = s.canvas;
  double half = s.width / 2;
  auto rounded = [&](double v) { return round(v * CURVE_POINT_DECIMALS) / CURVE_POINT_DECIMALS; };
  double frame_width = std::max({s.to - s.from, s.width + 0.05, FRAME_SIZE_MIN * canvas.width});
  double frame_height = std::max(s.width, FRAME_SIZE_MIN * canvas.height);
  CurveOptions o;
  o.id = s.id;
  o.frame = Box::from_pt({s.from, s.at - frame_height / 2, frame_width, frame_height}, canvas).frame();
  o.z_index = s.z_index;
  o.data = Json::object({{"mode", "samples"},
                         {"points", Json::array({Json::array({rounded(half), 0.5}),
                                                 Json::array({rounded(frame_width - half), 0.5})})},
                         {"domain", Json::array({0, rounded(frame_width)})},
                         {"yDomain", Json::array({0, 1})},
                         {"interpolation", "linear"}});
  o.style.color = s.color;
  o.style.opacity = s.opacity;
  o.style.stroke_width = s.width;
  o.style.line_cap = "round";
  return curve(o);
}
/** A visible length for `fraction` of `full`: proportional, but never shorter than one round cap. */
inline double fill_length(double fraction, double full, double minimum) {
  return fraction <= 0 ? 0 : std::min(full, std::max(minimum, fraction * full));
}
/** `label` option: true / string shows it, false hides it; absent keeps `fallback`. */
inline void parse_label(const Json &o, bool &show, std::optional<std::string> &text) {
  if (!options_detail::present(o, "label"))
    return;
  if (o["label"].is_string()) {
    show = true;
    text = o["label"].string();
  } else
    show = options_detail::boolean(o, "label");
}
} // namespace modern_detail
} // namespace glance_widgets
