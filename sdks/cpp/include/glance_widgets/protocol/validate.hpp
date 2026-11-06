#pragma once
#include "../color.hpp"
#include "constants.hpp"
#include "schemas.hpp"
namespace glance_widgets {
/** Resolve dotted object fields and array indices, returning null when absent. */
inline Json resolve_data_path(const Json &data, const std::string &path) {
  std::vector<std::pair<bool, std::string>> segments;
  if (!path_detail::scan_data_path(path, true, false, &segments))
    return {};
  Json current = data;
  for (const auto &segment : segments) {
    if (!segment.first)
      current = current.is_object() ? Json(current[segment.second]) : Json();
    else {
      const double index = std::strtod(segment.second.c_str(), nullptr);
      if (!current.is_array() || index >= static_cast<double>(current.size()))
        return {};
      current = Json(current.p->items[static_cast<std::size_t>(index)]);
    }
    if (current.is_null())
      return {};
  }
  return current;
}
namespace validation_detail {
[[noreturn]] inline void fail(const std::string &path, const std::string &message) {
  throw GlanceValidationError(path, message);
}
inline std::string at(const std::string &path, std::size_t index) { return path + "[" + std::to_string(index) + "]"; }
inline void check_rule(const Json &, const Json &, const std::string &);
inline void check_object(const Json &value, const Json &schema, const std::string &path) {
  if (!value.is_object())
    fail(path, "expected object");
  for (const auto &field : schema.p->fields)
    if (detail::has(value, field.first))
      check_rule(value[field.first], field.second, path + '.' + field.first);
}
inline void check_rule(const Json &value, const Json &rule, const std::string &path) {
  if (rule.is_string()) {
    const auto type = rule.string();
    bool ok = type == "textValue" ? value.is_string() || value.is_number()
              : type == "list" ? value.is_array()
              : type == "object" ? value.is_object()
              : type == "number"    ? value.is_number()
              : type == "integer" ? value.is_number() && std::floor(value.number()) == value.number()
              : type == "boolean" ? value.is_bool()
              : type == "string"  ? value.is_string()
                                  : value.is_string() && is_app_color(value.string());
    if (!ok)
      fail(path, "expected " + type);
  } else if (detail::has(rule, "enum")) {
    for (const auto &item : rule["enum"].p->items)
      if (detail::eq(value, item))
        return;
    fail(path, "unknown enum value " + value.dump());
  } else if (detail::has(rule, "arrayOf")) {
    if (!value.is_array())
      fail(path, "expected array");
    if (detail::has(rule, "length") && static_cast<double>(value.size()) != rule["length"].number())
      fail(path, "wrong array length");
    for (std::size_t i = 0; i < value.size(); ++i)
      check_rule(value.p->items[i], rule["arrayOf"], at(path, i));
  } else if (detail::has(rule, "object"))
    check_object(value, rule["object"], path);
  else {
    if (!value.is_object())
      fail(path, "expected object");
    for (const auto &field : value.p->fields)
      check_rule(field.second, rule["record"], path + '.' + field.first);
  }
}
inline void check_json(const Json &value, const std::string &path) {
  if (value.is_number()) {
    if (!std::isfinite(value.number()))
      fail(path, "non-finite number");
    if (std::abs(value.number()) >= MAX_SAFE_NATIVE_INTEGER)
      fail(path, "number too large for native renderer");
  } else if (value.is_array()) {
    for (std::size_t i = 0; i < value.size(); ++i)
      check_json(value.p->items[i], at(path, i));
  } else if (value.is_object()) {
    for (const auto &field : value.p->fields)
      check_json(field.second, path + '.' + field.first);
  }
}
inline void check_matrix(const Json &value, const std::string &path) {
  const auto &pixels = value["pixels"];
  if (!pixels.is_array() || !pixels.size() || !pixels[0].is_array())
    fail(path + ".pixels", "must be a non-empty 2D array");
  if (!pixels[0].size())
    fail(path + ".pixels[0]", "must contain at least one column");
  auto rows = pixels.size(), columns = pixels[0].size();
  if (rows > MAX_MATRIX_CELLS / columns)
    fail(path + ".pixels", "exceeds 4096 cells");
  for (std::size_t y = 0; y < rows; ++y) {
    auto row_path = at(path + ".pixels", y);
    const auto &row = pixels.p->items[y];
    if (!row.is_array() || row.size() != columns)
      fail(row_path, "inconsistent row size");
    for (std::size_t x = 0; x < columns; ++x)
      if (!row.p->items[x].is_null())
        check_object(row.p->items[x], pixel_schema, at(row_path, x));
  }
  if (detail::has(value, "default"))
    check_object(value["default"], pixel_schema, path + ".default");
  if (detail::has(value, "layout"))
    check_object(value["layout"], layout_schema, path + ".layout");
  if (!detail::has(value, "batch"))
    return;
  const auto &batch = value["batch"];
  if (!batch.is_array())
    fail(path + ".batch", "must be an array");
  for (std::size_t i = 0; i < batch.size(); ++i) {
    const auto &entry = batch.p->items[i];
    auto p = at(path + ".batch", i);
    check_object(entry, Json::object({{"pixel", object_rule(pixel_schema)}}), p);
    const auto &coordinates = entry["coordinates"];
    if (!coordinates.is_array())
      fail(p + ".coordinates", "must be an array");
    for (std::size_t j = 0; j < coordinates.size(); ++j) {
      const auto &pair = coordinates.p->items[j];
      auto pair_path = at(p + ".coordinates", j);
      check_rule(pair, array_rule("integer", 2), pair_path);
      if (pair[0].number() < 0 || pair[1].number() < 0 || pair[0].number() >= static_cast<double>(rows) ||
          pair[1].number() >= static_cast<double>(columns))
        fail(pair_path, "outside grid");
    }
  }
}
inline void check_fields(const Json &value, const Json &schema, const std::string &path) {
  if (!value.is_object()) fail(path, "expected object");
  for (const auto &field : value.p->fields) {
    if (!detail::has(schema, field.first)) fail(path + '.' + field.first, "unsupported field");
    if (!field.second.is_null()) check_rule(field.second, schema[field.first], path + '.' + field.first);
  }
}
inline void check_node(const Json &node, const std::string &type, const std::string &node_path, bool requires_frame) {
  check_json(node, node_path);
  if (!detail::has(layer_schemas, type)) fail(node_path, "unknown layer type");
  if (!node.is_object()) fail(node_path, "expected layer object");
  for (const auto &field : node.p->fields)
    if (field.first != "frame" && field.first != "zIndex" && field.first != "rotation" && field.first != "transform" && field.first != "content" && field.first != "style")
      fail(node_path + '.' + field.first, "unsupported field");
  const auto &frame = node["frame"];
  if (!frame.is_null() || requires_frame) {
    if (!frame.is_object() || !frame["width"].is_number() || !frame["height"].is_number()) fail(node_path + ".frame", "frame requires width and height");
    check_fields(frame, frame_schema, node_path + ".frame");
  }
  if (!node["zIndex"].is_null()) check_rule(node["zIndex"], "integer", node_path + ".zIndex");
  if (!node["rotation"].is_null()) check_rule(node["rotation"], "number", node_path + ".rotation");
  if (!node["transform"].is_null()) check_fields(node["transform"], Json::object({{"translateX", "number"}, {"translateY", "number"}}), node_path + ".transform");
  const auto &schema = layer_schemas[type];
  for (const auto *field : {"content", "style"})
    if (!node[field].is_null()) check_fields(node[field], schema[field], node_path + '.' + field);
  if (node["style"]["gradientColors"].is_array() && !node["style"]["gradientColors"].size()) fail(node_path + ".style.gradientColors", "gradient colors must not be empty");
  auto content = node["content"].is_null() ? Json::object() : node["content"];
  if (type == "matrix") {
    if (requires_frame && content["pixels"].is_null()) fail(node_path + ".content.pixels", "remote matrix needs pixels");
    if (content["pixels"].is_null() && !content["batch"].is_null()) fail(node_path + ".content.batch", "batch requires pixels");
    if (!content["layout"].is_null()) check_fields(content["layout"], layout_schema, node_path + ".content.layout");
    auto &fields = content.p->fields;
    fields.erase(std::remove_if(fields.begin(), fields.end(), [](const auto &v) { return v.second.is_null(); }), fields.end());
    if (!content["pixels"].is_null()) check_matrix(content, node_path + ".content");
    else if (!content["default"].is_null()) check_matrix(Json::object({{"pixels", Json::array({Json::array({Json()})})}, {"default", content["default"]}}), node_path + ".content");
  }
  if (type == "icon" || type == "image") {
    if (!content["icon"].is_null() && !content["url"].is_null()) fail(node_path + ".content", "one artwork source per node");
    if (content["icon"].is_null() && (!content["iconCodePoint"].is_null() || !content["iconFont"].is_null()))
      fail(node_path + ".content.icon", "glyph metadata requires icon");
  }
}

inline void check_layer(const Json &layer, const Json &data, const std::string &path) {
  if (!layer.is_object() || layer.size() != 3 || !detail::has(layer, "id") || !detail::has(layer, "type") || !detail::has(layer, "dataPath"))
    fail(path, "declaration requires only id, type, dataPath");
  if (!layer["id"].is_string() || layer["id"].string().rfind(LAYER_ID_PREFIX, 0) != 0 || layer["id"].string().size() == 7)
    fail(path + ".id", "must start with remote: and include a name");
  if (!layer["type"].is_string() || !detail::has(layer_schemas, layer["type"].string())) fail(path + ".type", "unknown layer type");
  if (!layer["dataPath"].is_string() || !path_detail::is_data_path(layer["dataPath"].string())) fail(path + ".dataPath", "invalid dataPath");
  auto node_path = layer["dataPath"].string();
  auto node = resolve_data_path(data, node_path);
  if (!node.is_object()) fail(node_path, "dataPath must resolve to a layer object");
  check_node(node, layer["type"].string(), node_path, true);
}

} // namespace validation_detail
/** Validate a full or partial node bound to a layer. */
inline void validate_layer_node(const Json &node, const std::string &type, const std::string &path = "$", bool requires_frame = false) {
  validation_detail::check_node(node, type, path, requires_frame);
}
/** Validate a complete protocol response, including exact UTF-8 serialized size. */
inline void validate(const Json &response) {
  using namespace validation_detail;
  if (!response.is_object())
    fail("$", "response must be a JSON object");
  check_json(response, "$");
  if (response.dump().size() > MAX_RESPONSE_BYTES)
    fail("$", "response exceeds 524288 bytes");
  if (detail::has(response, "protocol") && !detail::eq(response["protocol"], 1))
    fail("$.protocol", "must be 1");
  check_matrix(response, "$");
  Json data = options_detail::value(response, "data", Json::object());
  if (!data.is_object())
    fail("$.data", "must be an object");
  if (!detail::has(response, "layers"))
    return;
  const auto &layers = response["layers"];
  if (!layers.is_array())
    fail("$.layers", "must be an array");
  std::vector<std::string> ids;
  for (std::size_t i = 0; i < layers.size(); ++i) {
    check_layer(layers.p->items[i], data, at("$.layers", i));
    auto id = layers.p->items[i]["id"].string();
    if (std::find(ids.begin(), ids.end(), id) != ids.end()) fail(at("$.layers", i) + ".id", "duplicate layer id");
    ids.push_back(id);
  }
}
} // namespace glance_widgets
