#include "glance_widgets.hpp"
#include <fstream>
#include <iostream>
using namespace glance_widgets;
using detail::Args;
using detail::Value;
Json option(const Json& o, const std::string& k, Json fallback = {}) { return options_detail::value(o, k, fallback); }
DrawTextOptions draw_options(const Json& o) {
  DrawTextOptions d; static_cast<TextLayoutOptions&>(d) = font_detail::options(o);
  if (detail::has(o, "style")) d.style = pixel_detail::input(o["style"]);
  return d;
}
Matrix build_matrix(const Json& spec) {
  auto options = option(spec, "options", Json::object());
  if (options_detail::present(spec, "theme")) options = theme_options(get_theme(spec["theme"].string()), options);
  Matrix m(spec["columns"].number(), spec["rows"].number(), options);
  const auto operations = option(spec, "ops", Json::array());
  for (const auto& operation : operations.p->items) {
    const auto& a = operation.p->items; auto name = a[0].string();
    auto n = [&](std::size_t i) { return a.at(i).number(); };
    auto p = [&](std::size_t i) { return pixel_detail::input(a.at(i)); };
    auto o = [&](std::size_t i) { return i < a.size() ? a[i] : Json::object(); };
    auto fill = [&](std::size_t i) { return FillOptions{options_detail::boolean(o(i), "fill", true)}; };
    if (name == "set") m.set(n(1), n(2), p(3));
    else if (name == "fill") m.fill(p(1));
    else if (name == "clear") m.clear();
    else if (name == "line") m.line(n(1), n(2), n(3), n(4), p(5));
    else if (name == "rect") m.rect(n(1), n(2), n(3), n(4), p(5), fill(6));
    else if (name == "ellipse") m.ellipse(n(1), n(2), n(3), n(4), p(5), fill(6));
    else if (name == "circle") m.circle(n(1), n(2), n(3), p(4), fill(5));
    else if (name == "roundRect") m.round_rect(n(1), n(2), n(3), n(4), n(5), p(6), fill(7));
    else if (name == "arc") m.arc(n(1), n(2), n(3), n(4), n(5), p(6),
      {options_detail::integer(options_detail::number(o(7), "thickness", 1), "thickness", 1, 4096)});
    else if (name == "polygon") {
      std::vector<std::pair<int,int>> points;
      for (const auto& pair : detail::iter(a.at(1))) {
        if (pair.size() != 2) throw GlanceError("polygon points must be pairs");
        points.emplace_back(options_detail::integer(pair[0].number(), "x", -RASTER_MAX_COORDINATE, RASTER_MAX_COORDINATE),
          options_detail::integer(pair[1].number(), "y", -RASTER_MAX_COORDINATE, RASTER_MAX_COORDINATE));
      }
      m.polygon(points, p(2), fill(3));
    } else if (name == "floodFill") m.flood_fill(n(1), n(2), p(3), {options_detail::boolean(o(4), "diagonal")});
    else if (name == "text") m.text(a.at(1).string(), n(2), n(3), draw_options(o(4)));
    else if (name == "blit") m.blit(build_matrix(a.at(1)), n(2), n(3));
    else if (name == "flipX") m = m.flip_x();
    else if (name == "flipY") m = m.flip_y();
    else if (name == "rotate90") m = m.rotate90(a.size() > 1 ? n(1) : 1);
    else if (name == "crop") m = m.crop(n(1), n(2), n(3), n(4));
    else throw GlanceError("Unknown matrix operation: " + name);
  }
  return m;
}
Json ascii_picture(const Matrix& m) {
  std::vector<std::string> legend; const std::string alphabet = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ"; Args lines;
  for (int y = 0; y < m.rows(); ++y) {
    std::string line;
    for (int x = 0; x < m.columns(); ++x) {
      auto style = m.get(x, y);
      if (!m.is_set(x, y)) line += '.';
      else if (style.is_string()) line += '_';
      else {
        auto key = style.dump(); auto found = std::find(legend.begin(), legend.end(), key);
        if (found == legend.end()) { legend.push_back(key); found = legend.end() - 1; }
        line += alphabet.at(static_cast<std::size_t>(found - legend.begin()));
      }
    }
    lines.emplace_back(line);
  }
  return Json::array(lines);
}
Json run_call(const Json& c) {
  const auto name = c["fn"].string(); const auto& a = c["args"].p->items;
  auto o = [&](std::size_t i) { return i < a.size() ? a[i] : Json::object(); };
  if (name == "progressBar") return progress_bar(a.at(0).number(), o(1));
  if (name == "ringProgress") return ring_progress(a.at(0).number(), o(1));
  if (name == "battery") return battery(a.at(0).number(), o(1));
  if (name == "signal") return signal(a.at(0).number(), o(1));
  if (name == "pixelProgressBar") return pixel_progress_bar(a.at(0).number(), o(1));
  if (name == "labeledPixelProgressBar") return labeled_pixel_progress_bar(a.at(0).number(), o(1));
  if (name == "pixelRingProgress") return pixel_ring_progress(a.at(0).number(), o(1));
  if (name == "labeledPixelRingProgress") return labeled_pixel_ring_progress(a.at(0).number(), o(1));
  if (name == "statusStrip") return status_strip(a.at(0), o(1));
  if (name == "labeledStatusStrip") return labeled_status_strip(a.at(0), o(1));
  if (name == "heatmap") return heatmap(a.at(0), o(1));
  if (name == "calendarHeatmap") return calendar_heatmap(a.at(0), o(1));
  if (name == "labeledCalendarHeatmap") return labeled_calendar_heatmap(a.at(0), o(1));
  if (name == "barChart") return bar_chart(a.at(0), o(1));
  if (name == "pixelBarChart") return pixel_bar_chart(a.at(0), o(1));
  if (name == "pixelTextPanel") return pixel_text_panel(a.at(0).string(), o(1));
  if (name == "pixelTextRoot") return pixel_text_root(a.at(0).string(), o(1));
  if (name == "lineChart") return line_chart(a.at(0), o(1));
  if (name == "areaChart") return area_chart(a.at(0), o(1));
  if (name == "sparkline") return sparkline(a.at(0), o(1));
  if (name == "stat") return stat(a.at(0), o(1));
  if (name == "solveGrid") return solve_grid(a.at(0));
  if (name == "resolveFrame") return resolve_frame(a.at(0), a.at(1));
  if (name == "fitMatrix") return fit_matrix(a.at(0), a.at(1), a.at(2));
  if (name == "cellRect") return cell_rect(a.at(0), a.at(1).number(), a.at(2).number());
  if (name == "columnBoundary") return column_boundary(a.at(0), a.at(1).number());
  if (name == "rowBoundary") return row_boundary(a.at(0), a.at(1).number());
  if (name == "toRatioRect") return to_ratio_rect(a.at(0), a.at(1));
  if (name == "sign") return sign(a.at(0), a.at(1));
  if (name == "verifySignature") return verify_signature(a.at(0), a.at(1), o(2));
  if (name == "normalizeColor") return normalize_color(a.at(0).string());
  if (name == "withAlpha") return with_alpha(a.at(0).string(), a.at(1).number());
  if (name == "mixColors") return mix_colors(a.at(0).string(), a.at(1).string(), a.at(2).number());
  if (name == "merge") return merge_arguments(a);
  if (name == "toResponse") return to_response(a.at(0));
  if (name == "validate") { validate(a.at(0)); return Json(); }
  if (name == "text") return text(a.at(0));
  if (name == "line") return line(a.at(0));
  if (name == "shape") return shape(a.at(0));
  if (name == "icon") return icon(a.at(0));
  if (name == "image") return image(a.at(0));
  if (name == "curve") return curve(a.at(0));
  if (name == "horizontalLine") return horizontal_line(a.at(0));
  if (name == "verticalLine") return vertical_line(a.at(0));
  if (name == "iconGlyph") return icon_glyph(a.at(0).string()).to_json();
  if (name == "theme") return theme_json(get_theme(a.at(0).string()));
  if (name == "themeOptions") return theme_options(a.at(0), o(1));
  if (name == "measureText") return measure_text(a.at(0).string(), a.size() > 1 ? a[1] : Json(), o(2));
  if (name == "textMatrix") return ascii_picture(text_matrix(a.at(0).string(), o(1)));
  if (name == "matrix") {
    auto m = build_matrix(a.at(0)); auto mode = option(a[0], "export", "root").string();
    return mode == "ascii" ? ascii_picture(m) : mode == "part" ? m.to_part() : m.to_root();
  }
  if (name == "place") return place(build_matrix(a.at(0)).to_part(), a.at(1));
  if (name == "pixelate") {
    auto m = pixelate(a.at(0), a.at(1)); Args colors;
    for (int y = 0; y < m.rows(); ++y) for (int x = 0; x < m.columns(); ++x) {
      auto style = m.get(x,y); if (!m.is_set(x,y) || !style.is_object()) continue;
      auto color = style["color"];
      if (std::none_of(colors.begin(), colors.end(), [&](const Json& v) { return detail::eq(v, color); })) colors.push_back(color);
    }
    return Json::object({{"picture", ascii_picture(m)}, {"colors", Json::array(colors)}});
  }
  if (name == "boxFrame") {
    auto box = Box::canvas(option(a[0], "canvas"));
    const auto operations = option(a[0], "ops", Json::array());
    for (const auto& op : operations.p->items) {
      auto method = op[0].string();
      if (method == "inset") box = box.inset(Insets(op[1]));
      else if (method == "insetPt") box = box.inset_pt(Insets(op[1]));
      else if (method == "sub") box = box.sub(op[1].number(), op[2].number(), op[3].number(), op[4].number());
      else if (method == "rows" || method == "columns") {
        std::vector<double> weights;
        if (op[1].is_array()) for (const auto& v : op[1].p->items) weights.push_back(v.number());
        else weights.resize(static_cast<std::size_t>(options_detail::integer(op[1].number(), "count", 1, 100000)), 1);
        auto opts = op.size() > 3 ? op[2] : Json::object();
        SplitOptions s{options_detail::number(opts,"gap"), geometry_detail::number(opts,"gapPt")};
        auto parts = box.split(method == "rows", weights, s);
        auto index = options_detail::integer(op.p->items.back().number(), "index", 0, static_cast<int>(parts.size()) - 1);
        box = parts[static_cast<std::size_t>(index)];
      } else throw GlanceError("Unknown box operation: " + method);
    }
    return box.to_frame();
  }
  throw GlanceError("Unknown function: " + name);
}
std::string difference(const Value &a, const Value &b, const std::string &p = "$ ") {
  if (a.is_number() && b.is_number()) {
    if (std::isfinite(a.number()) &&
        std::abs(a.number() - b.number()) <= 1e-9 * std::max(1.0, std::abs(b.number())))
      return "";
    return p + ": " + a.dump() + " != " + b.dump();
  }
  if (a.p->kind != b.p->kind)
    return p + ": types differ";
  if (a.is_array()) {
    if (a.size() != b.size())
      return p + ": array lengths differ";
    for (std::size_t i = 0; i < a.size(); i++) {
      auto d = difference(a[int(i)], b[int(i)], p + "[" + std::to_string(i) + "]");
      if (!d.empty())
        return d;
    }
    return "";
  }
  if (a.is_object()) {
    if (a.size() != b.size())
      return p + ": object keys differ actual=" + a.dump() + " expected=" + b.dump();
    for (auto &kv : b.p->fields) {
      if (!detail::has(a, kv.first))
        return p + ": missing " + kv.first;
      auto d = difference(a[kv.first], kv.second, p + "." + kv.first);
      if (!d.empty())
        return d;
    }
    return "";
  }
  return detail::eq(a, b) ? "" : p + ": " + a.dump() + " != " + b.dump();
}
int main(int argc, char **argv) {
  int passed = 0, failed = 0;
  for (int i = 1; i < argc; i++) {
    std::ifstream f(argv[i]);
    if (!f) {
      std::cerr << "Missing golden file " << argv[i] << '\n';
      return 2;
    }
    std::stringstream content;
    content << f.rdbuf();
    auto cases = Value::parse(content.str());
    for (auto &c : detail::iter(cases)) {
      std::string diff;
      bool expect = detail::truth(option(c, "throws", false));
      try {
        auto r = run_call(detail::get(c, "call"));
        diff = expect ? "expected validation error" : difference(r, detail::get(c, "expected"));
      } catch (const std::exception &e) {
        std::string message = e.what();
        bool infrastructure = message.find("Unknown function:") != std::string::npos ||
                              message.find("Unknown attribute:") != std::string::npos ||
                              message.find("Value is not callable:") != std::string::npos;
        diff = expect && !infrastructure ? "" : message;
      }
      if (diff.empty())
        passed++;
      else {
        failed++;
        std::cerr << argv[i] << " / " << detail::get(c, "name").string() << ": " << diff << '\n';
      }
    }
  }
  std::cout << "C++ golden: " << passed << " passed, " << failed << " failed\n";
  return failed ? 1 : 0;
}
