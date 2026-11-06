#pragma once
#include "component_options.hpp"
namespace glance_widgets {
struct StatusStripOptions : ComponentOptions {
  int cell_width = 1, cell_gap = 1;
  std::optional<int> rows;
  std::map<std::string, std::string> colors;
};
struct LabeledStatusStripOptions : StatusStripOptions {
  std::optional<std::string> start_label, end_label;
  std::variant<bool, std::vector<std::string>> legend = false;
  std::map<std::string, std::string> names;
};
inline const Json STATUS_STRIP_DEFAULTS =
    Json::parse("{\"cellWidth\":1,\"cellGap\":1,\"rows\":5,\"colors\":{\"ok\":\"#34C759\",\"warn\":\"#FF"
                "9F0A\",\"down\":\"#FF453A\"},\"unknownState\":\"unknown\",\"names\":{\"ok\":\"OK\",\"war"
                "n\":\"Warn\",\"down\":\"Down\",\"unknown\":\"No data\"}}");
inline const Json LABELED_STATUS_STRIP_DEFAULTS =
    Json::parse(R"({"id":"statusStrip","fontSizeRatio":0.05,"labelOpacity":0.72,"zIndex":0})");
using StatusValue = std::optional<std::string>;
namespace status_detail {
inline std::map<std::string, std::string> palette(const std::map<std::string, std::string> &overrides) {
  std::map<std::string, std::string> colors{{"ok", "#34C759"}, {"warn", "#FF9F0A"}, {"down", "#FF453A"}};
  for (const auto &entry : overrides)
    colors[entry.first] = entry.second;
  return colors;
}
inline std::string default_name(const std::string &state) {
  if (state == "ok")
    return "OK";
  if (state == "warn")
    return "Warn";
  if (state == "down")
    return "Down";
  if (state == "unknown")
    return "No data";
  return state;
}
inline PixelInput style(const StatusValue &state, const PixelTheme &t,
                        const std::map<std::string, std::string> &colors) {
  std::string name = state.value_or("unknown");
  if (colors.find(name) != colors.end())
    return colors.at(name);
  if (name == "unknown")
    return t.default_style;
  throw GlanceError("No color for status: " + name);
}
inline Json strip(const std::vector<StatusValue> &states, const StatusStripOptions &o, std::optional<Rect> region) {
  using namespace component_detail;
  if (states.empty())
    throw GlanceError("statusStrip needs states");
  int w = positive(o.cell_width, "cellWidth"), gap = options_detail::integer(o.cell_gap, "cellGap", 0, 4096);
  auto t = o.theme;
  auto colors = status_detail::palette(o.colors);
  std::optional<double> rows = o.rows;
  if (rows)
    positive(*rows, "rows");
  else if (!region)
    rows = 5;
  double count = static_cast<double>(states.size());
  auto p = plan(region, count * w + (count - 1) * gap, rows, t);
  auto m = matrix(p, t);
  for (std::size_t i = 0; i < states.size(); ++i)
    m.rect(static_cast<double>(i) * (w + gap), 0, w, p.rows, style(states[i], t, colors));
  return m.to_part();
}
} // namespace status_detail
/** Render one colored cell group per status. */
inline Json status_strip(const std::vector<StatusValue> &states, const StatusStripOptions &o = {}) {
  return status_detail::strip(states, o, component_detail::region(o));
}
/** Add endpoint labels and an optional ordered legend. */
inline Json labeled_status_strip(const std::vector<StatusValue> &states, const LabeledStatusStripOptions &o) {
  using namespace component_detail;
  auto c = canvas(o);
  auto id = identity(o, "statusStrip");
  auto t = o.theme;
  auto colors = status_detail::palette(o.colors);
  colors.erase("label");
  auto names = o.names;
  double z = o.z_index.value_or(0), ratio = o.font_size_ratio.value_or(0.05);
  auto label_color = component_detail::color(o.colors, "label", t.palette.on);
  auto legend = o.legend;
  std::vector<std::string> listed;
  if (std::holds_alternative<std::vector<std::string>>(legend))
    for (const auto &s : std::get<std::vector<std::string>>(legend))
      listed.push_back(s);
  else if (std::get<bool>(legend))
    for (const auto &s : states) {
      auto n = s.value_or("unknown");
      if (std::find(listed.begin(), listed.end(), n) == listed.end())
        listed.push_back(n);
    }
  double pt = font_pt(ratio, c), row = text_height(pt), gap = pt * 0.4;
  bool labels = o.start_label.has_value() || o.end_label.has_value();
  double reserved = (labels ? gap + row : 0) + (listed.empty() ? 0 : gap + row);
  auto box = area(o, c);
  auto r = box.to_pt();
  if (reserved >= r.height)
    throw GlanceError("statusStrip area too small for labels");
  auto strip = box.inset_pt({0, 0, 0, reserved});
  auto opts = o;
  opts.colors = colors;
  auto placed =
      place_part(status_detail::strip(states, opts, emitted(strip, c)), id.first + ":strip", id.second, strip, z);
  detail::Args layers{placed};
  double top = (r.y + r.height) - reserved;
  auto add = [&](const std::string &role, double x, double width, const std::string &content, const std::string &align,
                 const std::string &ink) {
    layers.push_back(label(id.first + ":" + role, Box::from_pt({x, top, width, row}, c), content, align, "center",
                           ratio, ink, 0.72, z + 1));
  };
  if (labels) {
    top += gap;
    double half = r.width / 2;
    if (o.start_label.has_value())
      add("start", r.x, half, *o.start_label, "left", label_color);
    if (o.end_label.has_value())
      add("end", r.x + half, half, *o.end_label, "right", label_color);
    top += row;
  }
  if (!listed.empty()) {
    top += gap;
    double slot = r.width / static_cast<double>(listed.size()), dot = text_width("●", pt), offset = dot + pt * 0.25;
    for (std::size_t i = 0; i < listed.size(); ++i) {
      double left = r.x + static_cast<double>(i) * slot;
      auto ink = status_detail::style(listed[i], t, colors);
      auto swatch = std::holds_alternative<std::string>(ink) ? std::get<std::string>(ink) : t.default_style.color;
      add("legend-dot-" + std::to_string(i), left, dot, "●", "left", swatch);
      add("legend-name-" + std::to_string(i), left + offset, std::max(0.0, slot - offset),
          component_detail::color(names, listed[i], status_detail::default_name(listed[i])), "left", label_color);
    }
  }
  return composite(layers);
}
namespace status_detail {
inline std::vector<StatusValue> values(const Json &input) {
  std::vector<StatusValue> result;
  for (const auto &v : detail::iter(input))
    result.push_back(v.is_null() ? StatusValue{} : StatusValue{v.string()});
  return result;
}
inline StatusStripOptions options(const Json &input) {
  using namespace component_detail;
  StatusStripOptions o;
  static_cast<ComponentOptions &>(o) = common_options(input);
  o.cell_width = positive(number(input, "cellWidth", 1), "cellWidth");
  o.cell_gap = options_detail::integer(number(input, "cellGap", 1), "cellGap", 0, 4096);
  if (present(input, "rows"))
    o.rows = positive(number(input, "rows"), "rows");
  o.colors = color_options(input["colors"]);
  return o;
}
inline LabeledStatusStripOptions labeled_options(const Json &input) {
  using namespace component_detail;
  LabeledStatusStripOptions o;
  static_cast<StatusStripOptions &>(o) = options(input);
  o.start_label = layers_detail::optional_string(input, "startLabel");
  o.end_label = layers_detail::optional_string(input, "endLabel");
  o.names = color_options(input["names"]);
  if (present(input, "legend")) {
    if (input["legend"].is_array()) {
      std::vector<std::string> names;
      for (const auto &v : input["legend"].p->items)
        names.push_back(v.string());
      o.legend = names;
    } else
      o.legend = input["legend"].boolean();
  }
  return o;
}
} // namespace status_detail
inline Json status_strip(const Json &states, const Json &o = Json::object()) {
  return status_strip(status_detail::values(states), status_detail::options(o));
}
inline Json labeled_status_strip(const Json &states, const Json &o) {
  return labeled_status_strip(status_detail::values(states), status_detail::labeled_options(o));
}
} // namespace glance_widgets
