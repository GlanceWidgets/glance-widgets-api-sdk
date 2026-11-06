#pragma once
#include "generated/fonts.hpp"
#include "options.hpp"
#include <map>
#include <mutex>
namespace glance_widgets {
inline constexpr const char *DEFAULT_FONT_NAME = "3x5";
inline constexpr int DEFAULT_LETTER_SPACING = 1, DEFAULT_LINE_SPACING = 1, ANCHOR_BOX_WIDTH = 1;
inline constexpr char GLYPH_INK = '#';
struct Glyph {
  std::vector<std::string> rows;
  int width = 0, advance = 0, ink_bottom = 0;
};
struct GlyphDefinition {
  std::vector<std::string> rows;
  std::optional<int> advance;
};
struct FontDefinition {
  std::string name;
  int height = 0, descent = 0, spacing = 1;
  std::optional<int> advance;
  std::optional<std::string> fallback;
  std::vector<std::pair<std::string, GlyphDefinition>> glyphs;
  std::vector<std::pair<std::string, std::string>> aliases;
};
namespace font_detail {
inline int integer(double n, const std::string &label) { return options_detail::integer(n, label, -1000000, 1000000); }
inline FontDefinition definition(const Json &o) {
  FontDefinition d;
  d.name = options_detail::string(o, "name");
  d.height = integer(options_detail::number(o, "height"), "height");
  d.descent = integer(options_detail::number(o, "descent"), "descent");
  d.spacing = integer(options_detail::number(o, "spacing", 1), "spacing");
  if (options_detail::present(o, "advance"))
    d.advance = integer(o["advance"].number(), "advance");
  if (options_detail::present(o, "fallback"))
    d.fallback = o["fallback"].string();
  if (!o["glyphs"].is_object())
    throw GlanceError("Font glyphs must be an object");
  for (const auto &pair : o["glyphs"].p->fields) {
    const auto &value = pair.second;
    GlyphDefinition glyph;
    const auto &rows = value.is_array() ? value : value["rows"];
    for (const auto &row : detail::iter(rows))
      glyph.rows.push_back(row.string());
    if (value.is_object() && options_detail::present(value, "advance"))
      glyph.advance = integer(value["advance"].number(), "advance");
    d.glyphs.emplace_back(pair.first, glyph);
  }
  if (options_detail::present(o, "aliases"))
    for (const auto &pair : o["aliases"].p->fields)
      d.aliases.emplace_back(pair.first, pair.second.string());
  return d;
}
} // namespace font_detail
/** Compiled font. Glyph access returns copies so registry state cannot be mutated. */
class PixelFont {
  std::map<std::string, Glyph> glyphs_;
  std::optional<std::string> fallback_;

public:
  std::string name;
  int height, descent, spacing, line_height;
  explicit PixelFont(const FontDefinition &d)
      : fallback_(d.fallback), name(d.name), height(d.height), descent(d.descent), spacing(d.spacing), line_height(0) {
    if (height < 1 || descent < 0 || height > 1000000 || descent > 1000000)
      throw GlanceError("Font needs positive height and non-negative descent");
    line_height = height + descent;
    auto single = [](const std::string &c) {
      if (detail::chars(c).size() != 1)
        throw GlanceError("Glyph key must be one character");
    };
    for (const auto &pair : d.glyphs) {
      single(pair.first);
      const auto &definition = pair.second;
      Glyph g;
      g.rows = definition.rows;
      if (g.rows.size() > static_cast<std::size_t>(line_height))
        throw GlanceError("Too many glyph rows");
      g.width = g.rows.empty() ? 0 : font_detail::integer(static_cast<double>(g.rows[0].size()), "glyph width");
      for (const auto &row : g.rows) {
        if (row.size() != static_cast<std::size_t>(g.width) || row.find_first_not_of("#.") != std::string::npos)
          throw GlanceError("Invalid glyph row");
      }
      if (g.rows.empty() && !d.advance && !definition.advance)
        throw GlanceError("Empty glyph needs advance");
      g.advance = d.advance.value_or(definition.advance.value_or(g.width));
      if (g.advance < 0 || (d.advance && g.width > *d.advance))
        throw GlanceError("Invalid glyph advance");
      while (g.rows.size() < static_cast<std::size_t>(height))
        g.rows.emplace_back(static_cast<std::size_t>(g.width), '.');
      for (std::size_t i = 0; i < g.rows.size(); ++i)
        if (g.rows[i].find('#') != std::string::npos)
          g.ink_bottom = static_cast<int>(i) + 1;
      glyphs_[pair.first] = std::move(g);
    }
    for (const auto &pair : d.aliases) {
      single(pair.first);
      auto found = glyphs_.find(pair.second);
      if (found == glyphs_.end())
        throw GlanceError("Alias points at missing glyph");
      glyphs_.emplace(pair.first, found->second);
    }
    if (fallback_ && !has_glyph(*fallback_))
      throw GlanceError("Missing fallback glyph");
  }
  explicit PixelFont(const Json &d) : PixelFont(font_detail::definition(d)) {}
  int monospace_advance() const {
    int width = 0;
    for (const auto &entry : glyphs_) width = std::max(width, std::max(entry.second.width, entry.second.advance));
    return width;
  }
  bool has_glyph(const std::string &c) const { return glyphs_.count(c) != 0; }
  const Glyph &glyph_for(const std::string &c) const {
    auto it = glyphs_.find(c);
    if (it == glyphs_.end() && fallback_)
      it = glyphs_.find(*fallback_);
    if (it == glyphs_.end())
      throw GlanceError("Font " + name + " has no glyph for " + c);
    return it->second;
  }
};
namespace font_detail {
struct Registry {
  std::mutex mutex;
  std::map<std::string, PixelFont> fonts;
  Registry() {
    auto definitions = Json::parse(generated::fonts);
    for (const auto &d : definitions.p->items) {
      PixelFont font(d);
      fonts.emplace(font.name, font);
    }
  }
};
inline Registry &registry() {
  static Registry r;
  return r;
}
} // namespace font_detail
/** Register a named font, rejecting duplicates under the registry lock. */
inline PixelFont define_font(const FontDefinition &d) {
  PixelFont font(d);
  auto &r = font_detail::registry();
  std::lock_guard<std::mutex> lock(r.mutex);
  if (!r.fonts.emplace(font.name, font).second)
    throw GlanceError("Font already defined: " + font.name);
  return font;
}
inline PixelFont define_font(const Json &d) { return define_font(font_detail::definition(d)); }
inline PixelFont get_font(const std::string &name) {
  auto &r = font_detail::registry();
  std::lock_guard<std::mutex> lock(r.mutex);
  auto it = r.fonts.find(name);
  if (it == r.fonts.end())
    throw GlanceError("Unknown font: " + name);
  return it->second;
}
inline PixelFont resolve_font(const Json &input = {}) {
  return input.is_null()     ? get_font(DEFAULT_FONT_NAME)
         : input.is_string() ? get_font(input.string())
                             : PixelFont(input);
}
inline const PixelFont FONT_3X5 = get_font("3x5"), FONT_5X7 = get_font("5x7");
struct TextLayoutOptions {
  PixelFont font = FONT_3X5;
  std::optional<int> letter_spacing, max_width, box_width;
  bool monospace = false;
  int line_spacing = 1;
  std::string align = "left";
};
struct PositionedGlyph {
  Glyph glyph;
  double x, y;
};
struct TextLayout {
  double width = 0, height = 0;
  std::vector<std::string> lines;
  std::vector<PositionedGlyph> glyphs;
};
namespace font_detail {
inline TextLayoutOptions options(const Json &o) {
  TextLayoutOptions t;
  t.font = resolve_font(options_detail::value(o, "font"));
  if (options_detail::present(o, "letterSpacing"))
    t.letter_spacing = integer(o["letterSpacing"].number(), "letterSpacing");
  if (options_detail::present(o, "maxWidth"))
    t.max_width = integer(o["maxWidth"].number(), "maxWidth");
  if (options_detail::present(o, "boxWidth"))
    t.box_width = integer(o["boxWidth"].number(), "boxWidth");
  t.line_spacing = integer(options_detail::number(o, "lineSpacing", 1), "lineSpacing");
  t.monospace = options_detail::boolean(o, "monospace", false);
  t.align = options_detail::string(o, "align", "left");
  return t;
}
inline std::vector<std::string> split(const std::string &text, char delimiter) {
  std::vector<std::string> out;
  std::size_t from = 0;
  for (;;) {
    auto end = text.find(delimiter, from);
    out.push_back(text.substr(from, end - from));
    if (end == std::string::npos)
      return out;
    from = end + 1;
  }
}
} // namespace font_detail
/** Greedy word wrapping and Unicode code-point glyph placement. */
inline TextLayout layout_text(const std::string &text, const TextLayoutOptions &o = {}) {
  if (o.max_width && *o.max_width < 1)
    throw GlanceError("maxWidth must be at least 1");
  if (o.align != "left" && o.align != "center" && o.align != "right")
    throw GlanceError("Invalid text alignment");
  using Line = std::vector<std::string>;
  int spacing = o.letter_spacing.value_or(o.font.spacing);
  const int fixed = o.font.monospace_advance();
  auto width = [&](const Line &line) {
    double sum = 0;
    for (const auto &c : line)
      sum += o.monospace ? fixed : o.font.glyph_for(c).advance;
    return line.empty() ? 0 : sum + spacing * static_cast<double>(line.size() - 1);
  };
  std::vector<Line> lines;
  for (const auto &paragraph : font_detail::split(text, '\n')) {
    if (!o.max_width) {
      lines.push_back(detail::chars(paragraph));
      continue;
    }
    std::optional<Line> current;
    for (const auto &word : font_detail::split(paragraph, ' ')) {
      auto rest = detail::chars(word);
      Line candidate = current.value_or(Line{});
      if (current)
        candidate.push_back(" ");
      candidate.insert(candidate.end(), rest.begin(), rest.end());
      if (width(candidate) <= *o.max_width) {
        current = candidate;
        continue;
      }
      if (current)
        lines.push_back(*current);
      while (rest.size() > 1 && width(rest) > *o.max_width) {
        std::size_t count = 1;
        while (count < rest.size() &&
               width(Line(rest.begin(), rest.begin() + static_cast<std::ptrdiff_t>(count + 1))) <= *o.max_width)
          ++count;
        lines.emplace_back(rest.begin(), rest.begin() + static_cast<std::ptrdiff_t>(count));
        rest.erase(rest.begin(), rest.begin() + static_cast<std::ptrdiff_t>(count));
      }
      current = rest;
    }
    lines.push_back(current.value_or(Line{}));
  }
  TextLayout result;
  double pitch = double(o.font.line_height) + o.line_spacing, box = o.box_width.value_or(o.max_width.value_or(1));
  for (std::size_t i = 0; i < lines.size(); ++i) {
    double w = width(lines[i]);
    result.width = std::max(result.width, w);
    double pen = o.align == "left" ? 0 : o.align == "right" ? box - w : std::floor((box - w) / 2);
    std::string joined;
    for (const auto &c : lines[i]) {
      auto g = o.font.glyph_for(c);
      result.glyphs.push_back({g, pen + (o.monospace ? (fixed - g.width) / 2 : 0), static_cast<double>(i) * pitch});
      pen += double(o.monospace ? fixed : g.advance) + spacing;
      joined += c;
    }
    result.lines.push_back(joined);
  }
  int ink = o.font.height;
  for (const auto &c : lines.back())
    ink = std::max(ink, o.font.glyph_for(c).ink_bottom);
  result.height = static_cast<double>(lines.size() - 1) * pitch + ink;
  return result;
}
inline TextLayout layout_text(const std::string &text, const Json &o) {
  return layout_text(text, font_detail::options(o));
}
inline Json measure_text(const std::string &text, const Json &font = {}, Json o = Json::object()) {
  o["font"] = font;
  auto layout = layout_text(text, o);
  detail::Args lines;
  for (const auto &line : layout.lines)
    lines.emplace_back(line);
  return Json::object({{"width", layout.width}, {"height", layout.height}, {"lines", Json::array(lines)}});
}
inline Json measure_text(const std::string &text, const PixelFont &font, TextLayoutOptions options = {}) {
  options.font = font;
  auto layout = layout_text(text, options);
  return Json::object({{"width", layout.width}, {"height", layout.height}, {"lines", Json(layout.lines)}});
}
inline PixelFont resolve_font(const PixelFont &font) { return font; }
inline PixelFont resolve_font(const FontDefinition &definition) { return PixelFont(definition); }

} // namespace glance_widgets
