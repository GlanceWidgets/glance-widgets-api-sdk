#pragma once
#include "matrix.hpp"
namespace glance_widgets {
inline constexpr int DEFAULT_ALPHA_THRESHOLD = 128;
struct RgbaImage {
  int width, height;
  std::vector<std::uint8_t> data;
};
struct PixelateOptions {
  int columns = 1, rows = 1;
  std::optional<std::vector<std::string>> palette;
  double alpha_threshold = DEFAULT_ALPHA_THRESHOLD;
  std::string dither = "none";
  MatrixOptions matrix;
};
/** Average alpha-weighted RGBA source regions using integer arithmetic. */
inline Matrix pixelate(const RgbaImage &image, const PixelateOptions &o) {
  if (image.width < 1 || image.height < 1)
    throw GlanceError("Image size must be positive integers");
  if (static_cast<std::uint64_t>(image.width) * static_cast<unsigned>(image.height) * 4 != image.data.size())
    throw GlanceError("Image data length does not match RGBA dimensions");
  if (o.dither != "none")
    throw GlanceError("Unsupported dither");
  if (o.palette && o.palette->empty())
    throw GlanceError("palette must not be empty");
  std::vector<std::pair<std::string, Rgba>> palette;
  if (o.palette)
    for (const auto &color : *o.palette)
      palette.emplace_back(normalize_color(color), parse_color(color));
  Matrix matrix(o.columns, o.rows, o.matrix);
  auto rounded = [](std::int64_t n, std::int64_t d) { return static_cast<double>((2 * n + d) / (2 * d)); };
  for (int r = 0; r < o.rows; ++r)
    for (int c = 0; c < o.columns; ++c) {
      const std::int64_t top = std::int64_t(r) * image.height / o.rows;
      const std::int64_t bottom = std::max(top + 1, std::int64_t(r + 1) * image.height / o.rows);
      const std::int64_t left = std::int64_t(c) * image.width / o.columns;
      const std::int64_t right = std::max(left + 1, std::int64_t(c + 1) * image.width / o.columns);
      std::int64_t alpha = 0, red = 0, green = 0, blue = 0;
      for (auto y = top; y < bottom; ++y)
        for (auto x = left; x < right; ++x) {
          auto i = static_cast<std::size_t>((y * image.width + x) * 4);
          auto a = image.data[i + 3];
          alpha += a;
          red += image.data[i] * a;
          green += image.data[i + 1] * a;
          blue += image.data[i + 2] * a;
        }
      if (rounded(alpha, (bottom - top) * (right - left)) < o.alpha_threshold)
        continue;
      Rgba color = alpha ? Rgba{rounded(red, alpha), rounded(green, alpha), rounded(blue, alpha), 255} : Rgba{};
      std::string chosen = format_color(color);
      double best = std::numeric_limits<double>::infinity();
      for (const auto &entry : palette) {
        double dr = entry.second.r - color.r, dg = entry.second.g - color.g, db = entry.second.b - color.b;
        double distance = dr * dr + dg * dg + db * db;
        if (distance < best) {
          best = distance;
          chosen = entry.first;
        }
      }
      matrix.set(c, r, chosen);
    }
  return matrix;
}
inline Matrix pixelate(const Json &image, const Json &options) {
  auto integer = [](double n) { return options_detail::integer(n, "dimension", 1, 2147483647); };
  RgbaImage in{integer(image["width"].number()), integer(image["height"].number()), {}};
  for (const auto &value : detail::iter(image["data"]))
    in.data.push_back(static_cast<std::uint8_t>(options_detail::integer(value.number(), "RGBA byte", 0, 255)));
  PixelateOptions o;
  o.columns = integer(options["columns"].number());
  o.rows = integer(options["rows"].number());
  o.alpha_threshold = options_detail::number(options, "alphaThreshold", DEFAULT_ALPHA_THRESHOLD);
  o.dither = options_detail::string(options, "dither", "none");
  o.matrix = pixel_detail::options(options_detail::value(options, "matrix", Json::object()));
  if (options_detail::present(options, "palette")) {
    o.palette.emplace();
    for (const auto &value : detail::iter(options["palette"]))
      o.palette->push_back(value.string());
  }
  return pixelate(in, o);
}
} // namespace glance_widgets
