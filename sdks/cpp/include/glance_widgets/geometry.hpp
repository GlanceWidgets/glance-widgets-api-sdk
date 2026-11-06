#pragma once
#include "json.hpp"
#include "protocol/constants.hpp"
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>
namespace glance_widgets {
/** Geometry uses points; frames use canvas ratios. */
struct Rect {
  double x = 0, y = 0, width = 0, height = 0;
};
struct Canvas {
  double width = 0, height = 0;
};
struct Frame {
  std::optional<double> left, right, top, bottom, width, height;
};
inline Json frame_json(const Frame &frame) {
  Json result = Json::object();
  const std::pair<const char *, std::optional<double>> fields[] = {{"left", frame.left},   {"right", frame.right},
                                                                   {"top", frame.top},     {"bottom", frame.bottom},
                                                                   {"width", frame.width}, {"height", frame.height}};
  for (const auto &field : fields)
    if (field.second)
      result[field.first] = *field.second;
  return result;
}
struct MatrixLayout {
  std::string gap_mode = "default";
  std::optional<double> column_gap_ratio, row_gap_ratio;
  double cell_aspect_ratio = 1, edge_inset_ratio = 0;
};
struct GridSize {
  double columns = 0, rows = 0;
};
struct MatrixGeometry {
  double columns, rows, cell_width, cell_height, gap_x, gap_y, origin_x, origin_y;
};
struct SolveGridOptions {
  Rect region;
  std::optional<double> rows, columns, cell_size, gap;
  std::string gap_mode = "default";
  double edge_inset_ratio = 0;
  bool clamp_aspect = false;
};
/** Native solver result; serialize only when crossing the protocol boundary. */
struct SolvedGrid {
  double columns, rows;
  MatrixLayout layout;
  Json to_json() const {
    Json l = Json::object({{"gapMode", layout.gap_mode},
                           {"edgeInsetRatio", layout.edge_inset_ratio},
                           {"cellAspectRatio", layout.cell_aspect_ratio}});
    if (layout.column_gap_ratio)
      l["columnGapRatio"] = *layout.column_gap_ratio;
    if (layout.row_gap_ratio)
      l["rowGapRatio"] = *layout.row_gap_ratio;
    return Json::object({{"columns", columns}, {"rows", rows}, {"layout", l}});
  }
};
namespace geometry_detail {
inline constexpr double min_aspect = 0.1, max_aspect = 10, max_inset = 0.45;
inline constexpr double max_gap = 10, max_cells = 4096;
inline std::optional<double> number(const Json &value, const std::string &key) {
  if (!detail::has(value, key) || value[key].is_null())
    return {};
  if (!value[key].is_number())
    throw GlanceError("$." + key + ": expected number");
  return value[key].number();
}
inline double required(const Json &value, const std::string &key) {
  auto result = number(value, key);
  if (!result)
    throw GlanceError("$." + key + ": expected number");
  return *result;
}
/// Math.max / Math.min semantics: NaN propagates, +0 beats -0 for max (and vice versa for min).
inline double js_max(double a, double b) {
  if (std::isnan(a) || std::isnan(b))
    return std::numeric_limits<double>::quiet_NaN();
  return (a > b || (a == b && !std::signbit(a))) ? a : b;
}
inline double js_min(double a, double b) {
  if (std::isnan(a) || std::isnan(b))
    return std::numeric_limits<double>::quiet_NaN();
  return (a < b || (a == b && std::signbit(a))) ? a : b;
}
inline double clamp(double value, double lo, double hi) { return js_min(hi, js_max(lo, value)); }
inline std::optional<double> gap_ratio(const std::string &mode) {
  if (mode == "zero")
    return 0;
  if (mode == "near")
    return 0.08;
  if (mode == "default")
    return 0.16;
  if (mode == "far")
    return 0.28;
  if (mode == "extraFar")
    return 0.42;
  return {};
}
inline std::string mode(const Json &value) {
  if (!detail::has(value, "gapMode") || value["gapMode"].is_null())
    return "default";
  if (!value["gapMode"].is_string())
    throw GlanceError("$.gapMode: expected string");
  return value["gapMode"].string();
}
inline Frame frame(const Json &input) {
  return {number(input, "left"),   number(input, "right"), number(input, "top"),
          number(input, "bottom"), number(input, "width"), number(input, "height")};
}
inline Rect rect(const Json &value) {
  return {number(value, "x").value_or(0), number(value, "y").value_or(0), required(value, "width"),
          required(value, "height")};
}
inline Canvas canvas(const Json &value) { return {required(value, "width"), required(value, "height")}; }
inline MatrixLayout layout(const Json &value) {
  return {mode(value), number(value, "columnGapRatio"), number(value, "rowGapRatio"),
          number(value, "cellAspectRatio").value_or(1), number(value, "edgeInsetRatio").value_or(0)};
}
inline Json json(const Rect &value) {
  return Json::object({{"x", value.x}, {"y", value.y}, {"width", value.width}, {"height", value.height}});
}
inline MatrixGeometry geometry(const Json &value) {
  return {required(value, "columns"),    required(value, "rows"),   required(value, "cellWidth"),
          required(value, "cellHeight"), required(value, "gapX"),   required(value, "gapY"),
          required(value, "originX"),    required(value, "originY")};
}
} // namespace geometry_detail
/** Require positive canvas dimensions. */
inline Canvas require_canvas(Canvas canvas, const std::string &) {
  if (!std::isfinite(canvas.width) || !std::isfinite(canvas.height) || !(canvas.width > 0) || !(canvas.height > 0))
    throw GlanceError("Invalid canvas " + Json::object({{"width", canvas.width}, {"height", canvas.height}}).dump() +
                      ": width and height must be > 0");
  return canvas;
}
inline Json require_canvas(const Json &canvas, const std::string &what) {
  require_canvas(geometry_detail::canvas(canvas), what);
  return canvas;
}
/** Resolve anchors and clamped frame sizes into points. */
inline Rect resolve_frame(const Frame &frame, Canvas canvas) {
  canvas = require_canvas(canvas, "resolveFrame");
  auto axis = [](std::optional<double> start, std::optional<double> end, std::optional<double> size, double extent,
                 double fallback) {
    const double length = start && end ? geometry_detail::js_max(0.0, extent - *start * extent - *end * extent)
                                       : geometry_detail::clamp(size.value_or(fallback), 0.02, 1.5) * extent;
    const double position = start ? *start * extent : end ? extent - *end * extent - length : 0;
    return std::pair<double, double>{position, length};
  };
  auto x = axis(frame.left, frame.right, frame.width, canvas.width, 0.4);
  auto y = axis(frame.top, frame.bottom, frame.height, canvas.height, 0.16);
  return {x.first, y.first, x.second, y.second};
}
inline Json resolve_frame(const Json &frame, const Json &canvas) {
  using geometry_detail::number;
  return geometry_detail::json(
      resolve_frame(Frame{number(frame, "left"), number(frame, "right"), number(frame, "top"), number(frame, "bottom"),
                          number(frame, "width"), number(frame, "height")},
                    geometry_detail::canvas(canvas)));
}
/** Fit a matrix within a point region using the App's contain and center rules. */
inline MatrixGeometry fit_matrix(Rect region, GridSize grid, const MatrixLayout &layout) {
  using namespace geometry_detail;
  const double a = clamp(layout.cell_aspect_ratio, min_aspect, max_aspect), s = geometry_detail::js_min(a, 1.0);
  const double ratio = gap_ratio(layout.gap_mode).value_or(0.16);
  const double gx = clamp(layout.column_gap_ratio.value_or(ratio), 0, max_gap) * s;
  const double gy = clamp(layout.row_gap_ratio.value_or(ratio), 0, max_gap) * s;
  const double inset = clamp(layout.edge_inset_ratio, -max_inset, max_inset) * s;
  const double c = grid.columns, r = grid.rows;
  const double h =
      js_min(region.width / (c * a + (c - 1) * gx + 2 * inset), region.height / (r + (r - 1) * gy + 2 * inset));
  const double w = h * a, gap_x = h * gx, gap_y = h * gy, i = h * inset;
  return {c,
          r,
          w,
          h,
          gap_x,
          gap_y,
          region.x + (region.width - (c * w + (c - 1) * gap_x + 2 * i)) / 2 + i,
          region.y + (region.height - (r * h + (r - 1) * gap_y + 2 * i)) / 2 + i};
}
inline Json fit_matrix(const Json &region, const Json &grid, const Json &layout) {
  using namespace geometry_detail;
  auto g = fit_matrix(rect(region), GridSize{required(grid, "columns"), required(grid, "rows")},
                      geometry_detail::layout(layout));
  return Json::object({{"columns", g.columns},
                       {"rows", g.rows},
                       {"cellWidth", g.cell_width},
                       {"cellHeight", g.cell_height},
                       {"gapX", g.gap_x},
                       {"gapY", g.gap_y},
                       {"originX", g.origin_x},
                       {"originY", g.origin_y}});
}
/** Locate one cell in points. */
inline Rect cell_rect(const MatrixGeometry &g, double x, double y) {
  return {g.origin_x + x * (g.cell_width + g.gap_x), g.origin_y + y * (g.cell_height + g.gap_y), g.cell_width,
          g.cell_height};
}
inline Json cell_rect(const Json &g, double x, double y) {
  return geometry_detail::json(cell_rect(geometry_detail::geometry(g), x, y));
}
/** Boundaries are the centers of gaps. */
inline double column_boundary(const MatrixGeometry &g, double index) {
  return g.origin_x + index * (g.cell_width + g.gap_x) - g.gap_x / 2;
}
inline double row_boundary(const MatrixGeometry &g, double index) {
  return g.origin_y + index * (g.cell_height + g.gap_y) - g.gap_y / 2;
}
inline double column_boundary(const Json &g, double index) {
  return column_boundary(geometry_detail::geometry(g), index);
}
inline double row_boundary(const Json &g, double index) { return row_boundary(geometry_detail::geometry(g), index); }
/** Convert points to canvas ratios. */
inline Rect to_ratio_rect(Rect rect, Canvas canvas) {
  canvas = require_canvas(canvas, "toRatioRect");
  return {rect.x / canvas.width, rect.y / canvas.height, rect.width / canvas.width, rect.height / canvas.height};
}
inline Json to_ratio_rect(const Json &rect, const Json &canvas) {
  return geometry_detail::json(to_ratio_rect(geometry_detail::rect(rect), geometry_detail::canvas(canvas)));
}
/** Solve grid dimensions and cell aspect without unchecked integer conversions. */
inline SolvedGrid solve_grid(const SolveGridOptions &o) {
  using namespace geometry_detail;
  const auto &region = o.region;
  if (!(region.width > 0) || !(region.height > 0))
    throw GlanceError("solveGrid: region must have a positive size");
  auto mode_ratio = gap_ratio(o.gap_mode);
  if (!mode_ratio)
    throw GlanceError("solveGrid: unknown gapMode " + o.gap_mode);
  const double gap = o.gap.value_or(*mode_ratio), inset = o.edge_inset_ratio;
  if (!std::isfinite(gap) || gap < 0 || gap > max_gap)
    throw GlanceError("solveGrid: gap is outside 0–10");
  if (!std::isfinite(inset) || std::abs(inset) > max_inset)
    throw GlanceError("solveGrid: edgeInsetRatio is outside ±0.45");
  auto count = [&](double extent, double cell) {
    return js_max(1.0, std::floor((extent / cell + gap - 2 * inset) / (1 + gap) + 0.5));
  };
  auto cell = [&](double extent, double n) { return extent / (n + (n - 1) * gap + 2 * inset); };
  auto rows = o.rows, columns = o.columns;
  if (!rows && !columns) {
    if (!o.cell_size || !(*o.cell_size > 0))
      throw GlanceError("solveGrid: give rows, columns or a positive cellSize");
    rows = count(region.height, *o.cell_size);
  }
  if (!rows)
    rows = count(region.height, cell(region.width, *columns));
  if (!columns)
    columns = count(region.width, cell(region.height, *rows));
  for (auto entry : {std::pair<const char *, double>{"rows", *rows}, {"columns", *columns}})
    if (!std::isfinite(entry.second) || std::floor(entry.second) != entry.second || entry.second < 1)
      throw GlanceError(std::string("solveGrid: ") + entry.first + " must be a positive integer");
  const double c = *columns, r = *rows;
  if (r * c > max_cells)
    throw GlanceError("solveGrid: exceeds 4096 cells");
  const double k = region.width / region.height;
  const double ex = (c - 1) * gap + 2 * inset, ey = (r - 1) * gap + 2 * inset;
  double aspect = (k * (r + ey) - ex) / c;
  if (!(aspect >= 1)) {
    const double denominator = c + ex - k * ey;
    aspect = denominator <= 0 ? 0 : (k * r) / denominator;
    if (!(aspect > 0 && aspect <= 1))
      aspect = 0;
  }
  if (o.clamp_aspect)
    aspect = clamp(aspect, min_aspect, max_aspect);
  if (!std::isfinite(aspect) || aspect < min_aspect || aspect > max_aspect)
    throw GlanceError("solveGrid: grid cannot fill region (cellAspectRatio must stay within 0.1–10)");
  MatrixLayout layout;
  layout.gap_mode = o.gap_mode;
  layout.edge_inset_ratio = inset;
  layout.cell_aspect_ratio = aspect;
  if (o.gap) {
    layout.column_gap_ratio = gap;
    layout.row_gap_ratio = gap;
  }
  return {c, r, layout};
}
inline Json solve_grid(const Json &options) {
  using namespace geometry_detail;
  SolveGridOptions o;
  o.region = rect(options["region"]);
  o.rows = number(options, "rows");
  o.columns = number(options, "columns");
  o.cell_size = number(options, "cellSize");
  o.gap = number(options, "gap");
  o.gap_mode = mode(options);
  o.edge_inset_ratio = number(options, "edgeInsetRatio").value_or(0);
  o.clamp_aspect = options["clampAspect"].is_bool() && options["clampAspect"].boolean();
  return solve_grid(o).to_json();
}
} // namespace glance_widgets
