#include "glance_widgets/color.hpp"
#include <cassert>
#include <limits>

using namespace glance_widgets;
int main() {
  const auto c = parse_color("#80123456");
  assert(c.a == 128 && c.r == 18 && c.g == 52 && c.b == 86);
  assert(format_color(c) == "#80123456");
  assert(rgb(-1, 0.5, 300) == "#0001FF");
  assert(with_alpha("#336699", 0.5) == "#80336699");
  assert(mix_colors("#000", "#fff", 0.5) == "#808080");
  assert(mix_colors("#000", "#fff", -1) == "#000000");
  assert(mix_colors("#000", "#fff", 2) == "#FFFFFF");
  assert(normalize_color(u8"\u2000\ufeff#abc\u3000") == "#AABBCC");
  assert(!is_app_color("#abc"));
  assert(is_app_color("#AABBCC"));
  assert(!is_app_color(" #AABBCC "));
  bool threw = false;
  try { rgb(std::numeric_limits<double>::infinity(), 0, 0); }
  catch (const GlanceError &) { threw = true; }
  assert(threw);
  threw = false;
  try { mix_colors("#000", "#fff", std::numeric_limits<double>::quiet_NaN()); }
  catch (const GlanceError &) { threw = true; }
  assert(threw);
}
