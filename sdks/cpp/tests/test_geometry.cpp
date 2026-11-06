#include "glance_widgets/geometry.hpp"
#include <cassert>
using namespace glance_widgets;
int main() {
  SolveGridOptions options;
  options.region = {0, 0, 100, 100};
  options.rows = 10;
  auto result = solve_grid(options);
  assert(result.columns == 10);
  options.gap_mode = "constructor";
  bool rejected = false;
  try { solve_grid(options); } catch (const GlanceError&) { rejected = true; }
  assert(rejected);
  options.gap_mode = "default";
  options.rows.reset();
  options.cell_size = 1e-12;
  rejected = false;
  try { solve_grid(options); } catch (const GlanceError& e) {
    rejected = std::string(e.what()).find("4096 cells") != std::string::npos;
  }
  assert(rejected);
  auto a = Json::parse(R"({"region":{"width":100,"height":100},"rows":10,"gap":null})");
  auto b = Json::parse(R"({"region":{"width":100,"height":100},"rows":10})");
  assert(solve_grid(a).dump() == solve_grid(b).dump());
  a["gap"] = "0.1";
  rejected = false;
  try { solve_grid(a); } catch (const GlanceError&) { rejected = true; }
  assert(rejected);
  auto frame = resolve_frame(Frame{}, Canvas{100, 100});
  assert(frame.width == 40 && frame.height == 16);
}
