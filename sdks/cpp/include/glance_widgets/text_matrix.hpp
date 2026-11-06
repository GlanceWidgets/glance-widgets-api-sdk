#pragma once
#include "matrix.hpp"
namespace glance_widgets {
struct TextMatrixOptions {
  MatrixOptions matrix;
  DrawTextOptions text;
};
inline Matrix text_matrix(const std::string &content, TextMatrixOptions o = {}) {
  auto metrics = layout_text(content, o.text);
  Matrix matrix(std::max(1.0, metrics.width), std::max(1.0, metrics.height), o.matrix);
  o.text.box_width = font_detail::integer(metrics.width, "text width");
  return matrix.text(content, 0, 0, o.text);
}
inline Matrix text_matrix(const std::string &content, const Json &options) {
  TextMatrixOptions o;
  o.matrix = pixel_detail::options(options);
  static_cast<TextLayoutOptions &>(o.text) = font_detail::options(options);
  if (detail::has(options, "style"))
    o.text.style = pixel_detail::input(options["style"]);
  return text_matrix(content, o);
}
} // namespace glance_widgets
