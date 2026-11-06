#pragma once
#include "font.hpp"
#include "pixel.hpp"
#include "raster.hpp"
#include <map>
namespace glance_widgets {
struct DrawTextOptions : TextLayoutOptions {
  std::optional<PixelInput> style;
};
/** Mutable grid with typed cells and deterministic row-major batch export. */
class Matrix {
  using Cell = std::variant<std::nullptr_t, Empty, PixelStyle>;
  int columns_, rows_;
  MatrixLayout layout_;
  Cell default_;
  PixelStyle base_;
  std::vector<Cell> cells_;
  static bool is_empty_input(const PixelInput &p) {
    if (std::holds_alternative<Empty>(p))
      return true;
    const auto *word = std::get_if<std::string>(&p);
    return word != nullptr && *word == "empty";
  }
  Cell resolve(const PixelInput &p) const {
    if (std::holds_alternative<std::nullptr_t>(p))
      return nullptr;
    if (is_empty_input(p))
      return EMPTY;
    return pixel_detail::resolve(p, base_);
  }
  static Json protocol(const Cell &c) {
    if (std::holds_alternative<Empty>(c))
      return Json::object({{"empty", true}});
    if (auto *p = std::get_if<PixelStyle>(&c))
      return p->to_json();
    return {};
  }
  const Cell &effective(double x, double y) const {
    const auto &cell = cells_[index(x, y)];
    return std::holds_alternative<std::nullptr_t>(cell) ? default_ : cell;
  }
  static bool equal(const Cell &a, const Cell &b) {
    if (a.index() != b.index())
      return false;
    const auto *x = std::get_if<PixelStyle>(&a);
    if (!x)
      return true;
    const auto &y = std::get<PixelStyle>(b);
    return x->color == y.color && x->opacity == y.opacity && x->corner_radius == y.corner_radius &&
           x->rotation == y.rotation && x->halo == y.halo;
  }
  std::size_t index(double x, double y) const {
    if (!contains(x, y))
      throw GlanceError("Cell is outside matrix");
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(columns_) + static_cast<std::size_t>(x);
  }
  void plot(double x, double y, const Cell &c) {
    if (contains(x, y))
      cells_[index(x, y)] = c;
  }
  RasterBounds bounds() const { return {columns_, rows_}; }
  raster_detail::Emit emit(const Cell &c) {
    return [this, c](int x, int y) { plot(x, y, c); };
  }
  Matrix empty_like(double c, double r) const {
    MatrixOptions o;
    o.layout = layout_;
    o.base = pixel_overrides(base_);
    o.default_style =
        std::holds_alternative<Empty>(default_) ? PixelInput(EMPTY) : PixelInput(std::get<PixelStyle>(default_));
    return Matrix(c, r, o);
  }

public:
  Matrix(double columns, double rows, const MatrixOptions &options = {})
      : columns_(0), rows_(0), layout_(options.layout) {
    if (!std::isfinite(columns) || !std::isfinite(rows) || std::floor(columns) != columns || std::floor(rows) != rows ||
        columns < 1 || rows < 1)
      throw GlanceError("Matrix size must be positive integers");
    if (columns * rows > 4096)
      throw GlanceError("Matrix exceeds 4096 cells");
    columns_ = static_cast<int>(columns);
    rows_ = static_cast<int>(rows);
    pixel_detail::layout_json(layout_);
    default_ = is_empty_input(options.default_style) || std::holds_alternative<std::nullptr_t>(options.default_style)
                   ? Cell(EMPTY)
                   : Cell(pixel_detail::resolve(options.default_style));
    base_ = pixel_detail::resolve(options.base, std::holds_alternative<PixelStyle>(default_)
                                                    ? std::get<PixelStyle>(default_)
                                                    : DEFAULT_PIXEL_BASE);
    cells_.resize(static_cast<std::size_t>(columns_ * rows_), nullptr);
  }
  Matrix(double c, double r, const Json &o) : Matrix(c, r, pixel_detail::options(o)) {}
  int columns() const { return columns_; }
  int rows() const { return rows_; }
  MatrixLayout layout_options() const { return layout_; }
  Json layout() const { return pixel_detail::layout_json(layout_); }
  PixelStyle base() const { return base_; }
  bool contains(double x, double y) const {
    return x >= 0 && y >= 0 && x < columns_ && y < rows_ && std::floor(x) == x && std::floor(y) == y;
  }
  Json get(double x, double y) const {
    const auto &c = cells_[index(x, y)];
    const auto &effective = std::holds_alternative<std::nullptr_t>(c) ? default_ : c;
    return std::holds_alternative<Empty>(effective) ? Json("empty") : protocol(effective);
  }
  bool is_set(double x, double y) const { return !std::holds_alternative<std::nullptr_t>(cells_[index(x, y)]); }
  Matrix &set(double x, double y, const PixelInput &style) {
    cells_[index(x, y)] = resolve(style);
    return *this;
  }
  Matrix &fill(const PixelInput &style) {
    std::fill(cells_.begin(), cells_.end(), resolve(style));
    return *this;
  }
  Matrix &clear() { return fill(nullptr); }
  Matrix &blit(const Matrix &other, double x, double y) {
    for (int r = 0; r < other.rows_; ++r)
      for (int c = 0; c < other.columns_; ++c) {
        const auto &value = other.cells_[other.index(c, r)];
        if (std::holds_alternative<PixelStyle>(value))
          plot(x + c, y + r, value);
      }
    return *this;
  }
  Matrix &line(double x0, double y0, double x1, double y1, const PixelInput &style) {
    using raster_detail::coordinate;
    raster_detail::line(coordinate(x0), coordinate(y0), coordinate(x1), coordinate(y1), bounds(), emit(resolve(style)));
    return *this;
  }
  Matrix &rect(double x, double y, double w, double h, const PixelInput &style, FillOptions o = {}) {
    for (double n : {x, y, w, h})
      raster_detail::coordinate(n);
    auto cell = resolve(style);
    for (int row = 0; row < rows_; ++row)
      for (int col = 0; col < columns_; ++col) {
        if (col < x || row < y || col >= x + w || row >= y + h)
          continue;
        if (o.fill || col == x || row == y || col == x + w - 1 || row == y + h - 1)
          plot(col, row, cell);
      }
    return *this;
  }
  Matrix &ellipse(double x, double y, double w, double h, const PixelInput &style, FillOptions o = {}) {
    raster_detail::ellipse(raster_detail::coordinate(x), raster_detail::coordinate(y), raster_detail::extent(w),
                           raster_detail::extent(h), o.fill, bounds(), emit(resolve(style)));
    return *this;
  }
  Matrix &round_rect(double x, double y, double w, double h, double radius, const PixelInput &style,
                     FillOptions o = {}) {
    int r = options_detail::integer(radius, "radius", 0, 2147483647);
    raster_detail::round_rect(raster_detail::coordinate(x), raster_detail::coordinate(y), raster_detail::extent(w),
                              raster_detail::extent(h), r, o.fill, bounds(), emit(resolve(style)));
    return *this;
  }
  Matrix &circle(double x, double y, double radius, const PixelInput &style, FillOptions o = {}) {
    options_detail::integer(radius, "radius", 0, RASTER_MAX_EXTENT);
    return ellipse(x - radius, y - radius, 2 * radius + 1, 2 * radius + 1, style, o);
  }
  Matrix &arc(double x, double y, double radius, double start, double end, const PixelInput &style, ArcOptions o = {}) {
    int r = options_detail::integer(radius, "radius", 0, RASTER_MAX_EXTENT);
    options_detail::integer(o.thickness, "thickness", 1, RASTER_MAX_EXTENT);
    raster_detail::arc(raster_detail::coordinate(x), raster_detail::coordinate(y), r, start, end, o.thickness, bounds(),
                       emit(resolve(style)));
    return *this;
  }
  Matrix &polygon(const std::vector<std::pair<int, int>> &points, const PixelInput &style, FillOptions o = {}) {
    raster_detail::polygon(points, o.fill, bounds(), emit(resolve(style)));
    return *this;
  }
  Matrix &flood_fill(double x, double y, const PixelInput &style, FloodFillOptions o = {}) {
    raster_detail::coordinate(x);
    raster_detail::coordinate(y);
    if (!contains(x, y))
      return *this;
    Cell target = effective(x, y);
    Cell cell = resolve(style);
    std::vector<bool> visited(cells_.size());
    std::vector<std::size_t> pending{index(x, y)};
    visited[pending.back()] = true;
    constexpr int dx[] = {1, -1, 0, 0, 1, 1, -1, -1}, dy[] = {0, 0, 1, -1, 1, -1, 1, -1};
    while (!pending.empty()) {
      auto i = pending.back();
      pending.pop_back();
      int cx = static_cast<int>(i) % columns_, cy = static_cast<int>(i) / columns_;
      cells_[i] = cell;
      for (int j = 0; j < (o.diagonal ? 8 : 4); ++j) {
        int nx = cx + dx[j], ny = cy + dy[j];
        if (!contains(nx, ny))
          continue;
        auto k = index(nx, ny);
        if (!visited[k] && equal(effective(nx, ny), target)) {
          visited[k] = true;
          pending.push_back(k);
        }
      }
    }
    return *this;
  }
  Matrix &text(const std::string &content, double x, double y, const DrawTextOptions &o = {}) {
    raster_detail::coordinate(x);
    raster_detail::coordinate(y);
    auto cell = resolve(o.style.value_or(PixelInput(base_)));
    const auto layout = layout_text(content, o);
    for (const auto &p : layout.glyphs)
      for (std::size_t r = 0; r < p.glyph.rows.size(); ++r)
        for (std::size_t c = 0; c < p.glyph.rows[r].size(); ++c)
          if (p.glyph.rows[r][c] == '#')
            plot(x + p.x + static_cast<double>(c), y + p.y + static_cast<double>(r), cell);
    return *this;
  }
  Matrix clone() const { return *this; }
  Matrix flip_x() const {
    auto out = empty_like(columns_, rows_);
    for (int y = 0; y < rows_; ++y)
      for (int x = 0; x < columns_; ++x)
        out.cells_[out.index(columns_ - 1 - x, y)] = cells_[index(x, y)];
    return out;
  }
  Matrix flip_y() const {
    auto out = empty_like(columns_, rows_);
    for (int y = 0; y < rows_; ++y)
      for (int x = 0; x < columns_; ++x)
        out.cells_[out.index(x, rows_ - 1 - y)] = cells_[index(x, y)];
    return out;
  }
  Matrix rotate90(double turns = 1) const {
    auto out = *this;
    int count = options_detail::integer(turns, "turns", -2147483647, 2147483647);
    for (int n = (count % 4 + 4) % 4; n > 0; --n) {
      auto next = out.empty_like(out.rows_, out.columns_);
      for (int y = 0; y < out.rows_; ++y)
        for (int x = 0; x < out.columns_; ++x)
          next.cells_[next.index(out.rows_ - 1 - y, x)] = out.cells_[out.index(x, y)];
      out = std::move(next);
    }
    return out;
  }
  Matrix crop(double x, double y, double w, double h) const {
    raster_detail::coordinate(x);
    raster_detail::coordinate(y);
    auto out = empty_like(w, h);
    for (int r = 0; r < out.rows_; ++r)
      for (int c = 0; c < out.columns_; ++c)
        if (contains(x + c, y + r))
          out.cells_[out.index(c, r)] = cells_[index(x + c, y + r)];
    return out;
  }
  Json to_root() const {
    // Cells are grouped by their serialized pixel, exactly like TS's JSON.stringify key, so that
    // numerically equal values (0/-0, 1/1.0) share a group while emitted values stay as drawn.
    struct Group {
      Json pixel;
      std::vector<std::pair<std::size_t, std::size_t>> coordinates;
    };
    std::vector<Group> groups;
    std::map<std::string, std::size_t> group_of;
    auto def = protocol(default_);
    const std::string default_key = def.dump();
    for (std::size_t r = 0; r < static_cast<std::size_t>(rows_); ++r)
      for (std::size_t c = 0; c < static_cast<std::size_t>(columns_); ++c) {
        const auto &cell = cells_[r * static_cast<std::size_t>(columns_) + c];
        if (std::holds_alternative<std::nullptr_t>(cell))
          continue;
        auto p = protocol(cell);
        auto key = p.dump();
        if (key == default_key)
          continue;
        auto found = group_of.find(key);
        if (found == group_of.end()) {
          found = group_of.emplace(key, groups.size()).first;
          groups.push_back({std::move(p), {}});
        }
        groups[found->second].coordinates.emplace_back(r, c);
      }
    detail::Args grid;
    for (int r = 0; r < rows_; ++r)
      grid.push_back(Json::array(detail::Args(static_cast<std::size_t>(columns_))));
    detail::Args batch;
    for (const auto &group : groups) {
      if (group.coordinates.size() >= static_cast<std::size_t>(BATCH_MIN_OCCURRENCES)) {
        detail::Args coordinates;
        for (const auto &pair : group.coordinates)
          coordinates.push_back(Json::array({Json(static_cast<double>(pair.first)), Json(static_cast<double>(pair.second))}));
        batch.push_back(Json::object({{"pixel", group.pixel}, {"coordinates", Json::array(coordinates)}}));
      } else
        for (const auto &pair : group.coordinates)
          grid[pair.first].p->items[pair.second] = group.pixel;
    }
    auto root = Json::object({{"pixels", Json::array(grid)}, {"default", def}, {"layout", layout()}});
    if (!batch.empty())
      root["batch"] = Json::array(batch);
    return root;
  }
  Json to_part() const {
    return Json::object({{"content", to_root()}});
  }
};
} // namespace glance_widgets
