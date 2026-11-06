#pragma once
#include "component_options.hpp"
#include "pixel_fit.hpp"
namespace glance_widgets {
struct StatColors {
  std::optional<std::string> value, label, good, bad, neutral;
};
struct StatOptions : ComponentOptions {
  std::optional<std::string> label, unit, delta_text, delta_suffix;
  std::optional<double> delta, max_cell_size;
  std::optional<int> decimals;
  int delta_decimals = 1;
  bool invert_delta = false;
  std::string arrow = "icon", value_style = "text", align = "left";
  PixelFont pixel_font = FONT_5X7, pixel_unit_font = FONT_3X5;
  StatColors colors;
};
inline const Json STAT_DEFAULTS =
    Json::parse("{\"id\":\"stat\",\"deltaDecimals\":1,\"arrow\":\"icon\",\"valueStyle\":\"text\",\"align"
                "\":\"left\",\"fontSizeRatio\":0.08,\"pixelFont\":\"5x7\",\"pixelUnitFont\":\"3x5\",\"z"
                "Index\":0,\"labelOpacity\":0.72,\"valueColor\":\"#FFFFFF\",\"labelColor\":\"#FFFFF"
                "F\",\"upIcon\":\"ph_arrowUp\",\"downIcon\":\"ph_arrowDown\",\"flatIcon\":\"ph_minus\""
                "}");
using StatValue = std::variant<double, std::string>;
/** Lay out a value, optional label, unit and delta. */
inline Json stat(const StatValue &value, const StatOptions &o) {
  using namespace component_detail;
  auto c = canvas(o);
  auto id = identity(o, "stat");
  if (!std::holds_alternative<std::string>(value) &&
      (!std::holds_alternative<double>(value) || !std::isfinite(std::get<double>(value))))
    throw GlanceError("stat value must be finite or string");
  auto decimals = [&](double n) { return options_detail::integer(n, "decimals", 0, 100); };
  std::string value_text =
      std::holds_alternative<std::string>(value)
          ? std::get<std::string>(value)
          : format(
                std::get<double>(value),
                decimals(o.decimals.value_or(std::floor(std::get<double>(value)) == std::get<double>(value) ? 0 : 1)));
  auto t = o.theme;
  bool pixel = o.value_style == "pixel", center = o.align == "center";
  double z = o.z_index.value_or(0);
  auto colors = o.colors;
  auto value_color = colors.value.value_or(pixel ? t.palette.on : "#FFFFFF");
  auto delta = o.delta;
  std::optional<std::string> direction, delta_content;
  if (delta) {
    direction = *delta > 0 ? "up" : *delta < 0 ? "down" : "flat";
    delta_content = (*delta > 0 ? "+" : "") + format(*delta, decimals(o.delta_decimals)) + o.delta_suffix.value_or("");
  }
  if (o.delta_text.has_value())
    delta_content = *o.delta_text;
  auto arrow = o.arrow;
  std::vector<std::string> roles;
  std::vector<double> weights;
  if (o.label.has_value()) {
    roles.push_back("label");
    weights.push_back(2);
  }
  roles.push_back("value");
  weights.push_back(5);
  if (delta_content) {
    roles.push_back("delta");
    weights.push_back(2);
  }
  auto boxes = area(o, c).rows(weights);
  auto row = [&](const std::string &role) {
    return boxes[static_cast<std::size_t>(std::find(roles.begin(), roles.end(), role) - roles.begin())].to_pt();
  };
  detail::Args layers;
  auto horizontal = center ? "center" : "left";
  auto add = [&](const std::string &role, Rect r, const std::string &content, const std::string &align, double pt,
                 const std::string &ink, double opacity, const std::string &weight) {
    layers.push_back(label(id.first + ":" + role, Box::from_pt(r, c), content, align, "center", font_ratio(pt, c), ink,
                           opacity, z, weight));
  };
  auto used = [&](double pt) { return font_pt(font_ratio(pt, c), c); };
  auto fit = [&](double h, double w, const std::string &s) {
    return used(std::min({h / label_lines(), fit_font(s, w), font_pt(o.font_size_ratio.value_or(0.08), c)}));
  };
  auto append = [&](const Json &part, const std::string &role, const std::string &path, const Box &box) {
    auto placed = place_part(part, id.first + ":" + role, path, box, z);
    layers.push_back(placed);
  };
  if (o.label.has_value()) {
    auto r = row("label");
    auto s = *o.label;
    add("label", r, s, horizontal, fit(r.height, r.width, s), colors.label.value_or("#FFFFFF"), 0.72, "400");
  }
  auto r = row("value");
  auto unit = o.unit;
  if (pixel) {
    auto font = o.pixel_font, small_font = o.pixel_unit_font;
    TextLayoutOptions main_options;
    main_options.font = font;
    TextLayoutOptions unit_options;
    unit_options.font = small_font;
    auto main = layout_text(value_text, main_options);
    auto small = unit ? layout_text(*unit, unit_options) : TextLayout{};
    double mw = main.width, mh = main.height, sw = unit ? small.width : 0, sh = unit ? small.height : 0;
    auto content = content_matrix(mw + (unit ? 1 + sw : 0), std::max(mh, sh), t);
    DrawTextOptions draw;
    draw.font = resolve_font(font);
    draw.style = value_color;
    content.text(value_text, 0, 0, draw);
    if (unit) {
      draw.font = resolve_font(small_font);
      draw.style = colors.value.value_or(t.palette.dim);
      content.text(*unit, mw + 1, std::max(0.0, mh - sh), draw);
    }
    auto box = Box::from_pt(r, c);
    auto m =
        fit_pixel_content(content, emitted(box, c), {t, center ? "center" : "left", "center", false, o.max_cell_size});
    append(m.to_part(), "value", id.second, box);
  } else {
    double em = advance(value_text),
           width_em = unit ? std::max(em * 1.05, em + 0.15 + advance(*unit) * 1.05 * 0.45) : em * 1.05;
    double fit_width = std::max(0.0, r.width - 1) / std::max(1e-9, width_em),
           pt = used(std::min(r.height / 1.32, fit_width));
    double vw = advance(value_text) * pt, uf = used(pt * 0.45), gap = pt * 0.15, uw = unit ? advance(*unit) * uf : 0;
    double total = vw + (unit ? gap + uw : 0), start = center ? r.x + (r.width - total) / 2 : r.x, h = pt * 1.32,
           top = r.y + (r.height - h) / 2;
    add("value", {start, top, text_width(value_text, pt), h}, value_text, "left", pt, value_color, 1, "700");
    if (unit) {
      double uh = uf * 1.32;
      add("unit", {start + vw + gap, top + h - uh, text_width(*unit, uf), uh}, *unit, "left", uf, value_color, 0.72,
          "400");
    }
  }
  if (delta_content) {
    auto dr = row("delta");
    std::optional<bool> good;
    if (direction && *direction != "flat")
      good = (*direction == "up") != o.invert_delta;
    auto ink = !good   ? colors.neutral.value_or("#8E8E93")
               : *good ? colors.good.value_or("#34C759")
                       : colors.bad.value_or("#FF453A");
    bool show = arrow != "none" && direction.has_value();
    double pt = fit(dr.height, dr.width - (show ? 1.2 * dr.height : 0), *delta_content);
    double arrow_pt = show ? pt * 1.2 : 0, gap = show ? pt * 0.4 : 0,
           total = arrow_pt + gap + advance(*delta_content) * pt;
    double start = center ? dr.x + (dr.width - total) / 2 : dr.x;
    if (show) {
      auto box = Box::from_pt({start, dr.y + (dr.height - arrow_pt) / 2, arrow_pt, arrow_pt}, c);
      if (arrow == "icon") {
        IconOptions icon_options;
        icon_options.id = id.first + ":arrow";
        icon_options.frame = box.frame();
        icon_options.z_index = z;
        icon_options.name = *direction == "up" ? "ph_arrowUp" : *direction == "down" ? "ph_arrowDown" : "ph_minus";
        icon_options.style.color = ink;
        layers.push_back(icon(icon_options));
      } else {
        std::vector<std::string> rows = *direction == "flat"
                                            ? std::vector<std::string>{".....", ".....", "#####", ".....", "....."}
                                            : std::vector<std::string>{"..#..", ".###.", "#####", "..#..", "..#.."};
        if (*direction == "down")
          std::reverse(rows.begin(), rows.end());
        auto m = content_matrix(5, 5, t);
        for (int y = 0; y < 5; ++y)
          for (int x = 0; x < 5; ++x)
            if (rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] == '#')
              m.set(x, y, ink);
        append(m.to_part(), "arrow", id.second + "Arrow", box);
      }
    }
    add("delta", {start + arrow_pt + gap, dr.y, text_width(*delta_content, pt), dr.height}, *delta_content, "left", pt,
        ink, 1, "600");
  }
  return composite(layers);
}
namespace stat_detail {
inline StatOptions options(const Json &input) {
  using namespace component_detail;
  StatOptions o;
  static_cast<ComponentOptions &>(o) = common_options(input);
  o.label = layers_detail::optional_string(input, "label");
  o.unit = layers_detail::optional_string(input, "unit");
  o.delta_text = layers_detail::optional_string(input, "deltaText");
  o.delta_suffix = layers_detail::optional_string(input, "deltaSuffix");
  o.delta = geometry_detail::number(input, "delta");
  o.max_cell_size = geometry_detail::number(input, "maxCellSize");
  if (present(input, "decimals"))
    o.decimals = options_detail::integer(number(input, "decimals"), "decimals", 0, 100);
  o.delta_decimals = options_detail::integer(number(input, "deltaDecimals", 1), "deltaDecimals", 0, 100);
  o.invert_delta = boolean(input, "invertDelta");
  o.arrow = string(input, "arrow", "icon");
  o.value_style = string(input, "valueStyle", "text");
  o.align = string(input, "align", "left");
  o.pixel_font = resolve_font(component_detail::value(input, "pixelFont", "5x7"));
  o.pixel_unit_font = resolve_font(component_detail::value(input, "pixelUnitFont", "3x5"));
  o.colors.value = layers_detail::optional_string(input["colors"], "value");
  o.colors.label = layers_detail::optional_string(input["colors"], "label");
  o.colors.good = layers_detail::optional_string(input["colors"], "good");
  o.colors.bad = layers_detail::optional_string(input["colors"], "bad");
  o.colors.neutral = layers_detail::optional_string(input["colors"], "neutral");
  return o;
}
} // namespace stat_detail
inline Json stat(const Json &value, const Json &options) {
  StatValue input = value.is_string() ? StatValue{value.string()} : StatValue{value.number()};
  return stat(input, stat_detail::options(options));
}
} // namespace glance_widgets
