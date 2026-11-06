#pragma once
#include "shared.hpp"
namespace glance_widgets {
struct PixelFitOptions {
  PixelTheme theme = themes.lcd;
  std::string align = "center", valign = "center";
  bool unlit = false;
  std::optional<double> max_cell_size;
};
namespace component_detail {
inline Matrix content_matrix(double columns, double rows, const PixelTheme &t) {
  return matrix({std::max(1.0, columns), std::max(1.0, rows), t.layout}, t);
}
inline Matrix fit_pixel_content(const Matrix &content, Rect region, const PixelFitOptions &o) {
  auto ratio = geometry_detail::gap_ratio(o.theme.layout.gap_mode);
  if (!ratio)
    throw GlanceError("Unknown gapMode: " + o.theme.layout.gap_mode);
  double gap = *ratio;
  auto cell_for = [&](double extent, double count) { return extent / (count + (count - 1) * gap); };
  double cell = std::min(cell_for(region.width, content.columns()), cell_for(region.height, content.rows()));
  if (o.max_cell_size) {
    if (!(*o.max_cell_size > 0))
      throw GlanceError("maxCellSize must be positive");
    cell = std::min(cell, *o.max_cell_size / (1 + gap));
  }
  auto count = [&](double extent) { return std::floor((extent / cell + gap) / (1 + gap) + 1e-9); };
  double columns = std::max(double(content.columns()), count(region.width)),
         rows = std::max(double(content.rows()), count(region.height));
  if (columns * rows > 4096)
    throw GlanceError("pixel content exceeds 4096 cells");
  SolveGridOptions solve;
  solve.region = region;
  solve.columns = columns;
  solve.rows = rows;
  solve.gap_mode = o.theme.layout.gap_mode;
  auto grid = solve_grid(solve);
  auto m =
      matrix({columns, rows, grid.layout}, o.theme, o.unlit ? PixelInput(o.theme.default_style) : PixelInput(EMPTY));
  auto offset = [](double slack, const std::string &pos, const std::string &start, const std::string &end) {
    return pos == start ? 0 : pos == end ? slack : std::floor(slack / 2);
  };
  return m.blit(content, offset(columns - content.columns(), o.align, "left", "right"),
                offset(rows - content.rows(), o.valign, "top", "bottom"));
}
} // namespace component_detail
} // namespace glance_widgets
