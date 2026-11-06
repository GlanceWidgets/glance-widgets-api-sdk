#include "glance_widgets.hpp"
#include <cassert>
#include <iostream>
using namespace glance_widgets;
int main() {
  for (const auto *json : {"\"\\ud800\"", "\"\\udfff\"",
                           "\"\\ud800x\"", "\"\\ud800\\ud800\""}) {
    assert(Json::parse(json).dump() == json);
  }
  assert(Json::parse("\"\\ud83d\\ude00\"").dump() == "\"😀\"");
  bool invalid_json = false;
  try { Json::parse("[1,]"); }
  catch (const GlanceError &) { invalid_json = true; }
  assert(invalid_json);

  assert(std::isinf(Json::parse("1e400").number()));
  assert(Json::parse("-1e400").number() < 0);
  assert(Json::parse("5e-324").number() == std::numeric_limits<double>::denorm_min());
  assert(Json::parse("1e-400").number() == 0);
  Matrix m(4, 3);
  m.set(0, 0, "#abc").line(0, 2, 3, 2, "#fff");
  assert(m.columns() == 4 && m.rows() == 3 && m.get(0, 0)["color"].string() == "#AABBCC");
  auto mirrored = m.flip_x();
  mirrored.set(3, 0, "#f00");
  assert(m.get(0, 0)["color"].string() == "#AABBCC");
  auto response = to_response(m.to_root());
  assert(response["protocol"].number() == 1);
  auto canvas = Json::object({{"width", 200}, {"height", 100}});
  auto frame = Box::canvas(canvas).inset_pt(10).to_frame();
  assert(frame["left"].number() == .05);
  auto placed =
      place(pixel_progress_bar(70, Json::object({{"segments", 10}, {"thickness", 3}})),
            Json::object(
                {{"id", "progress"}, {"path", "data.progress"}, {"frame", Box::canvas(canvas)}}));
  validate(to_response(placed));
  auto parsed = Json::parse("{\"empty\":{},\"list\":[],\"null\":null,\"unicode\":\"你😀\"}");
  assert(parsed.dump() == "{\"empty\":{},\"list\":[],\"null\":null,\"unicode\":\"你😀\"}");
  auto duplicate = Json::parse("{\"k\":1,\"other\":0,\"k\":2}");
  assert(duplicate.size() == 2 && duplicate["k"].number() == 2);
  assert(duplicate.dump() == "{\"k\":2,\"other\":0}");
  auto nested_duplicate = Json::parse("{\"outer\":{\"k\":1,\"k\":2}}");
  assert(nested_duplicate["outer"].size() == 1);
  assert(nested_duplicate["outer"]["k"].number() == 2);
  auto constructed_duplicate = Json::object({{"k", 1}, {"k", 2}});
  assert(constructed_duplicate.size() == 1 && constructed_duplicate["k"].number() == 2);
  assert(measure_text("😀", "3x5")["width"].number() == 3);
  assert(FONT_3X5.has_glyph("A"));
  assert(sign("1700000000000", "this is secret") == "sSFWELbV2YwjdDQhWZwTcWlX5BWUx5J6TPpsZmuPii0=");
  assert(verify_signature(
      Json::object({{"timestamp", "1700000000000"}, {"signature", sign("1700000000000", "s")}}),
      "s", Json::object({{"now", 1700000000000.0}})));
  bool rejected = false;
  try {
    to_response(Json::object({{"pixels", Json::array({Json::array({false})})}}));
  } catch (const GlanceValidationError &e) {
    rejected = e.path == "$.pixels[0][0]";
  }
  assert(rejected);
  assert(detail::fixed(2.5, 0) == "3");
  assert(detail::fixed(-.125, 2) == "-0.13");
  assert(detail::fixed(1.005, 2) == "1.00");
  assert(detail::fixed(.615, 2) == "0.61");
  auto custom = Json::parse("{\"name\":\"custom\",\"height\":1,\"glyphs\":{\"你\":[\"#\"]}} ");
  auto font = define_font(custom);
  assert(font.has_glyph("你"));
  assert(measure_text("你", font)["width"].number() == 1);
  std::vector<double> values{1, 2, 3};
  auto chart = pixel_bar_chart(
      values, Json::object({{"canvas", canvas}, {"showValueLabels", false}, {"showAxes", false}}));
  validate(to_response(chart));
  validate(to_response(bar_chart(values, Json::object({{"canvas", canvas}}))));
  validate(to_response(battery(70, Json::object({{"canvas", canvas}, {"label", true}}))));
  validate(to_response(signal(50, Json::object({{"canvas", canvas}}))));
  validate(to_response(ring_progress(50, Json::object({{"canvas", canvas}}))));
  validate(to_response(progress_bar(50, Json::object({{"canvas", canvas}, {"label", true}}))));
  for (const auto &pair : std::vector<std::pair<double, std::string>>{
           {1e21, "1e+21"},
           {1.2345678901234567e21, "1.2345678901234568e+21"},
           {1e20, "100000000000000000000"},
           {1e-7, "1e-7"},
           {1e-6, "0.000001"}}) {
    assert(detail::numstr(pair.first) == pair.second);
    assert(sign(pair.first, "secret") == sign(pair.second, "secret"));
    if (pair.first >= 1e21) {
      auto card = stat(pair.first, Json::object({{"canvas", canvas}}));
      bool found = false;
      for (const auto &layer : detail::iter(card["layers"]))
        if (layer["id"].string() == "remote:stat:value") {
          found = true;
          assert(resolve_data_path(card["data"], layer["dataPath"].string())["content"]["text"].string() == pair.second);
        }
      assert(found);
    }
  }

  // Serialized byte count includes UTF-8 and JSON escaping.
  auto rejects = [](const auto& action) {
    try { action(); } catch (const GlanceError&) { return true; }
    return false;
  };
  for (const std::string &unit : {std::string("汉"), std::string("😀"), std::string("\\")}) {
    std::string huge;
    for (int i = 0; i < 300000; ++i) huge += unit;
    auto body = empty_root(); body["data"] = Json::object({{"large", huge}});
    assert(rejects([&] { validate(body); }));
  }
  Matrix clipped(8, 8);
  clipped.line(-1e9, 0, 1e9, 8, std::string("#fff"));
  clipped.rect(-1e9, -1e9, 2000000000, 2000000000, std::string("#fff"));
  auto independent = clipped.to_root(); independent["layout"]["gapMode"] = "far";
  assert(clipped.layout()["gapMode"].string() == "default");
  assert(rejects([&] { Box::canvas().rows(-1); }));
  assert(with_alpha("#336699", std::string("0.5")) == "#80336699");
  for (const auto* type : {"constructor", "toString"}) {
    auto body = empty_root();
    body["layers"] = Json::array({Json::object({{"id", "remote:test"}, {"type", type}})});
    assert(rejects([&] { validate(body); }));
  }
  auto source = Json::object({{"data", Json::object({{"key", 1}})}});
  auto copy = merge(source, Json::object({{"onConflict", nullptr}}));
  copy["data"]["key"] = 2;
  assert(source["data"]["key"].number() == 1);
  assert(layer_path("remote:stat:value") == "data.layer_stat_3a_value");
  assert(layer_path("a_b") == "data.layer_a_5f_b");
  assert(layer_path("😀") == "data.layer__1f600_");
  auto part = text(Json::object({{"id", "node"}, {"frame", Box(0, 0, 1, 1)}, {"text", "hello"}}));
  auto next = text(Json::object({{"id", "node"}, {"frame", Box(0, 0, 1, 1)}, {"text", "world"}}));
  auto composed = merge(part, next);
  assert(composed["layers"].size() == 1 && composed["layers"][0].size() == 3);
  validate(to_response(composed));
  composed["data"]["layer_node"]["content"]["text"] = "changed";
  assert(resolve_data_path(part["data"], "data.layer_node")["content"]["text"].string() == "hello");
  validate_layer_node(Json::parse(R"({"content":{"text":42},"style":{"autoWrap":false,"textOpacity":0}})"), "text");
  validate_layer_node(Json::parse(R"({"content":{"default":{"opacity":0.5}}})"), "matrix");
  validate_layer_node(Json::parse(R"({"style":{"color":null}})"), "text");
  assert(rejects([&] { validate_layer_node(Json::parse(R"({"frame":{"left":0.1}})"), "text"); }));
  assert(rejects([&] { validate_layer_node(Json::parse(R"({"content":{"batch":[]}})"), "matrix"); }));
  assert(rejects([&] { validate_layer_node(Json::parse(R"({"style":{"gradientColors":[]}})"), "shape"); }));
  assert(rejects([&] { validate_layer_node(Json::parse(R"({"content":{"sourceMode":"api"}})"), "text"); }));
  assert(rejects([&] { validate_layer_node(Json::parse(R"({"content":{"url":"a","icon":"b"}})"), "icon"); }));
  std::cout << "C++ native API checks passed\n";
}
