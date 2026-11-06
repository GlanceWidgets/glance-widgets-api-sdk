#pragma once
#include "options.hpp"
#include <functional>
namespace glance_widgets {
inline constexpr int RASTER_MAX_EXTENT = 4096, RASTER_MAX_COORDINATE = 1 << 24, ARC_DIRECTION_SCALE = 1 << 20;
struct RasterBounds {
  int columns, rows;
};
namespace raster_detail {
using Coord = std::int64_t;
using Emit = std::function<void(int, int)>;
inline Coord coordinate(double value) { return options_detail::integer(value, "coordinate", -2147483647, 2147483647); }
/** Closed-form Bresenham steps preserve the unclipped line's tie decisions. */
inline void line(Coord x0, Coord y0, Coord x1, Coord y1, RasterBounds b, const Emit &emit) {
  Coord dx = std::abs(x1 - x0), dy = std::abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
  bool horizontal = dx >= dy;
  Coord major = horizontal ? dx : dy, minor = horizontal ? dy : dx;
  Coord start = horizontal ? x0 : y0, sign = horizontal ? sx : sy;
  int limit = horizontal ? b.columns : b.rows;
  for (int coordinate = 0; coordinate < limit; ++coordinate) {
    Coord step = (coordinate - start) * sign;
    if (step < 0 || step > major)
      continue;
    Coord offset = major == 0
                       ? 0
                       : static_cast<Coord>((static_cast<std::uint64_t>(step) * static_cast<std::uint64_t>(minor) +
                                             static_cast<std::uint64_t>(major / 2)) /
                                            static_cast<std::uint64_t>(major));
    Coord x = horizontal ? coordinate : x0 + offset * sx;
    Coord y = horizontal ? y0 + offset * sy : coordinate;
    if (x >= 0 && x < b.columns && y >= 0 && y < b.rows)
      emit(static_cast<int>(x), static_cast<int>(y));
  }
}
template <class Contains>
inline void mask(Coord x, Coord y, int w, int h, bool fill, RasterBounds b, Contains contains, const Emit &emit) {
  auto inside = [&](Coord i, Coord j) { return i >= 0 && j >= 0 && i < w && j < h && contains(i, j); };
  for (int row = 0; row < b.rows; ++row)
    for (int col = 0; col < b.columns; ++col) {
      Coord i = col - x, j = row - y;
      if (inside(i, j) && (fill || !inside(i - 1, j) || !inside(i + 1, j) || !inside(i, j - 1) || !inside(i, j + 1)))
        emit(col, row);
    }
}
inline int extent(double n) { return options_detail::integer(n, "raster extent", 1, RASTER_MAX_EXTENT); }
inline void ellipse(Coord x, Coord y, int w, int h, bool fill, RasterBounds b, const Emit &emit) {
  Coord ww = Coord(w) * w, hh = Coord(h) * h;
  mask(
      x, y, w, h, fill, b,
      [=](Coord i, Coord j) {
        Coord dx = 2 * i + 1 - w, dy = 2 * j + 1 - h;
        return dx * dx * hh + dy * dy * ww <= ww * hh;
      },
      emit);
}
inline void round_rect(Coord x, Coord y, int w, int h, int radius, bool fill, RasterBounds b, const Emit &emit) {
  Coord r = std::min(radius, std::min(w, h) / 2);
  mask(
      x, y, w, h, fill, b,
      [=](Coord i, Coord j) {
        Coord qi = std::min(i, w - 1 - i), qj = std::min(j, h - 1 - j);
        if (qi >= r || qj >= r)
          return true;
        Coord dx = 2 * qi + 1 - 2 * r, dy = 2 * qj + 1 - 2 * r;
        return dx * dx + dy * dy <= 4 * r * r;
      },
      emit);
}
inline void arc(Coord cx, Coord cy, int radius, double start, double end, int thickness, RasterBounds b,
                const Emit &emit) {
  if (!std::isfinite(start) || !std::isfinite(end))
    throw GlanceError("arc angles must be finite");
  double sweep = end - start;
  if (!(sweep > 0))
    return;
  auto direction = [](double angle) {
    constexpr double pi = 3.14159265358979323846;
    double radians = angle * pi / 180;
    if (!std::isfinite(radians))
      throw GlanceError("arc angle out of range");
    return std::pair<Coord, Coord>{static_cast<Coord>(std::floor(std::sin(radians) * ARC_DIRECTION_SCALE + 0.5)),
                                   -static_cast<Coord>(std::floor(std::cos(radians) * ARC_DIRECTION_SCALE + 0.5))};
  };
  auto a = direction(start), z = direction(end);
  Coord outer = Coord(radius) * radius + radius, r = radius - thickness, inner = r >= 0 ? r * r + r : -1;
  for (int y = 0; y < b.rows; ++y)
    for (int x = 0; x < b.columns; ++x) {
      Coord dx = x - cx, dy = y - cy;
      if (std::abs(dx) > radius || std::abs(dy) > radius)
        continue;
      Coord d = dx * dx + dy * dy;
      bool after = a.first * dy - a.second * dx >= 0, before = dx * z.second - dy * z.first >= 0;
      if (d <= outer && d > inner && (sweep >= 360 || (sweep < 180 ? after && before : after || before)))
        emit(x, y);
    }
}
inline void polygon(const std::vector<std::pair<int, int>> &points, bool fill, RasterBounds b, const Emit &emit) {
  if (points.size() < 3)
    throw GlanceError("polygon needs at least 3 points");
  for (std::size_t i = 0; i < points.size(); ++i) {
    auto a = points[i], z = points[(i + 1) % points.size()];
    for (auto n : {a.first, a.second})
      if (n < -RASTER_MAX_COORDINATE || n > RASTER_MAX_COORDINATE)
        throw GlanceError("polygon coordinate out of range");
    line(a.first, a.second, z.first, z.second, b, emit);
  }
  if (!fill)
    return;
  for (int y = 0; y < b.rows; ++y)
    for (int x = 0; x < b.columns; ++x) {
      int right = 0, through = 0;
      Coord cy = 2 * y + 1;
      for (std::size_t i = 0; i < points.size(); ++i) {
        auto a = points[i], z = points[(i + 1) % points.size()];
        if ((2LL * a.second <= cy) == (2LL * z.second <= cy))
          continue;
        Coord dy = Coord(z.second) - a.second;
        Coord t = (2LL * a.first - 2LL * x - 1) * dy + (cy - 2LL * a.second) * (Coord(z.first) - a.first);
        if (!t)
          ++through;
        else if ((t > 0) == (dy > 0))
          ++right;
      }
      if (right % 2 || (right + through) % 2)
        emit(x, y);
    }
}
} // namespace raster_detail
} // namespace glance_widgets
