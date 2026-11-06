#pragma once
#include "modern.hpp"
#include "pixel_ring_progress.hpp"
namespace glance_widgets {
struct RingProgressColors {
  std::string fill = modern_detail::FILL_COLOR, track = modern_detail::TRACK_COLOR, label = modern_detail::LABEL_COLOR;
};
struct RingProgressOptions : ComponentOptions {
  double min = 0, max = 100, start_angle = 0;
  /** Ring width in points, 0.5-12. Default 10% of the ring's diameter, within 3-12. */
  std::optional<double> thickness;
  bool counter_clockwise = false, show_label = true;
  std::optional<std::string> label;
  RingProgressColors colors;
};
/** The arc from `from` to `to` (degrees, clockwise from 12 o'clock) as x-ascending polylines on the unit circle. */
inline std::vector<std::vector<std::pair<double, double>>> arc_pieces(double from, double to) {
  using component_detail::round;
  constexpr double step_degrees = 2, decimals = 1e4, x_extreme = 90, half_turn = 180;
  auto unit = [&](double v) { return round(std::min(1.0, std::max(-1.0, v)) * decimals) / decimals; };
  std::vector<double> bounds{from};
  for (double angle = std::ceil((from - x_extreme) / half_turn) * half_turn + x_extreme; angle < to;
       angle += half_turn)
    if (angle > from)
      bounds.push_back(angle);
  bounds.push_back(to);
  std::vector<std::vector<std::pair<double, double>>> pieces;
  for (std::size_t index = 0; index + 1 < bounds.size(); ++index) {
    double a = bounds[index], b = bounds[index + 1];
    if (!(b - a > 1e-9))
      continue;
    double count = std::max(2.0, std::ceil((b - a) / step_degrees) + 1);
    std::vector<std::pair<double, double>> points;
    for (double step = 0; step < count; ++step) {
      double radians = ((a + ((b - a) * step) / (count - 1)) * 3.141592653589793) / half_turn;
      points.emplace_back(unit(std::sin(radians)), unit(std::cos(radians)));
    }
    if (points.front().first > points.back().first)
      std::reverse(points.begin(), points.end());
    pieces.push_back(std::move(points));
  }
  return pieces;
}
/** Modern progress ring built from `curve` layers; canvas required. */
inline Json ring_progress(double value, const RingProgressOptions &o) {
  using namespace component_detail;
  using namespace modern_detail;
  auto c = canvas(o);
  auto id = identity(o, "ring");
  range(o.min, o.max);
  if (!std::isfinite(o.start_angle))
    throw GlanceError("ringProgress: startAngle must be finite, got " + detail::numstr(o.start_angle));
  double z = o.z_index.value_or(0);
  double f = fraction(value, o.min, o.max);
  auto a = area(o, c).to_pt();
  double side = std::min(a.width, a.height);
  Rect ring{a.x + (a.width - side) / 2, a.y + (a.height - side) / 2, side, side};
  auto ring_box = Box::from_pt(ring, c);
  double thickness =
      o.thickness.value_or(std::min(CURVE_STROKE_MAX_PT, std::max(3.0, round(side * 0.1))));
  require_stroke("ringProgress", "thickness", thickness, CURVE_STROKE_MAX_PT);
  if (!(thickness < side / 2))
    throw GlanceError("ringProgress: a " + detail::numstr(thickness) + " pt ring does not fit " +
                      detail::numstr(side) + " pt");
  detail::Args layers;
  auto pieces = [&](double from, double to, const std::string &role, const std::string &color, double opacity,
                    double zi) {
    auto arcs = arc_pieces(from, to);
    for (std::size_t index = 0; index < arcs.size(); ++index) {
      detail::Args points;
      for (auto &p : arcs[index])
        points.push_back(Json::array({p.first, p.second}));
      CurveOptions curve_options;
      curve_options.id = id.first + ":" + role + "-" + std::to_string(index);
      curve_options.frame = ring_box.frame();
      curve_options.z_index = zi;
      curve_options.data = Json::object({{"mode", "samples"},
                                         {"points", Json::array(points)},
                                         {"domain", Json::array({-1, 1})},
                                         {"yDomain", Json::array({-1, 1})},
                                         {"interpolation", "linear"}});
      curve_options.style.color = color;
      curve_options.style.opacity = opacity;
      curve_options.style.stroke_width = thickness;
      curve_options.style.line_cap = "round";
      // The stroke is centered on the circle: pad by half of it so it stays inside the frame.
      curve_options.style.plot_padding = thickness / 2;
      layers.push_back(curve(curve_options));
    }
  };
  pieces(-90, 270, "track", o.colors.track, TRACK_OPACITY, z);
  if (f > 0) {
    double sweep = f * 360;
    double from = o.counter_clockwise ? o.start_angle - sweep : o.start_angle;
    double to = o.counter_clockwise ? o.start_angle : o.start_angle + sweep;
    pieces(from, to, "arc", o.colors.fill, 1, z + 1);
  }
  if (o.show_label) {
    auto content = o.label.value_or(percent_label(f));
    double hole_radius = std::max(1.0, side / 2 - thickness);
    double hole_width = 1.6 * hole_radius, hole_height = 1.2 * hole_radius;
    double requested = o.font_size_ratio.has_value()
                           ? font_pt(*o.font_size_ratio, c)
                           : std::min(hole_height / label_lines(), fit_font(content, hole_width));
    double pt = font_pt(font_ratio(requested, c), c);
    double w = std::max(hole_width, text_width(content, pt)), h = std::max(hole_height, text_height(pt));
    double cx = ring.x + side / 2, cy = ring.y + side / 2;
    layers.push_back(label(id.first + ":label", Box::from_pt({cx - w / 2, cy - h / 2, w, h}, c), content, "center",
                           "center", font_ratio(pt, c), o.colors.label, 1, z + 2));
  }
  return composite(layers);
}
inline Json ring_progress(double value, const Json &input = Json::object()) {
  using namespace component_detail;
  RingProgressOptions o;
  static_cast<ComponentOptions &>(o) = common_options(input);
  o.min = number(input, "min", 0);
  o.max = number(input, "max", 100);
  o.start_angle = number(input, "startAngle", 0);
  o.thickness = geometry_detail::number(input, "thickness");
  o.counter_clockwise = boolean(input, "counterClockwise");
  modern_detail::parse_label(input, o.show_label, o.label);
  const auto &colors = input["colors"];
  o.colors.fill = string(colors, "fill", o.colors.fill);
  o.colors.track = string(colors, "track", o.colors.track);
  o.colors.label = string(colors, "label", o.colors.label);
  return ring_progress(value, o);
}
} // namespace glance_widgets
