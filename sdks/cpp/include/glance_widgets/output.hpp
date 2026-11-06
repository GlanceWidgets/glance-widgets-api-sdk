#pragma once
#include "box.hpp"
#include "protocol/validate.hpp"
namespace glance_widgets {
/** Conflict handling for data leaves, layer ids and the root matrix. */
struct MergeOptions {
  bool error_on_conflict = false;
};
struct PlaceOptions {
  std::string id;
  Frame frame;
  std::string path;
  std::optional<double> z_index, rotation;
  Json transform;
};
/** Prefix an id exactly once. */
inline std::string layer_id(const std::string &id) {
  if (id.empty())
    throw GlanceError("Layer id must be a non-empty string");
  return id.rfind(LAYER_ID_PREFIX, 0) == 0 ? id : std::string(LAYER_ID_PREFIX) + id;
}
/** Deterministic node path for a layer id. */
inline std::string layer_path(const std::string &id) {
  auto value = layer_id(id).substr(7);
  std::string path = "data.layer_";
  for (std::size_t i = 0; i < value.size();) {
    auto first = static_cast<unsigned char>(value[i++]);
    unsigned code = first;
    unsigned remaining = first < 0x80 ? 0 : first < 0xe0 ? 1 : first < 0xf0 ? 2 : 3;
    if (remaining) code &= (1u << (6u - remaining)) - 1u;
    while (remaining-- && i < value.size()) code = (code << 6u) | (static_cast<unsigned char>(value[i++]) & 0x3fu);
    if ((code >= 'A' && code <= 'Z') || (code >= 'a' && code <= 'z') || (code >= '0' && code <= '9'))
      path += static_cast<char>(code);
    else { std::ostringstream hex; hex << std::hex << code; path += '_' + hex.str() + '_'; }
  }
  return path;
}
/** Nest data at a dotted object path. */
inline Json nest_at_path(const std::string &path, Json value) {
  if (!path_detail::scan_data_path(path, false, true))
    throw GlanceError("Invalid data path: expected data.name(.name)*");
  std::size_t end = path.size();
  while (end > 4) {
    auto dot = path.rfind('.', end - 1);
    value = Json::object({{path.substr(dot + 1, end - dot - 1), value}});
    end = dot;
  }
  return value;
}
namespace output_detail {
inline void merge_data(Json &target, const Json &source, bool error, const std::string &path) {
  for (const auto &field : source.p->fields) {
    const auto &key = field.first;
    if (detail::has(target, key) && target[key].is_object() && field.second.is_object())
      merge_data(target[key], field.second, error, path + '.' + key);
    else {
      if (error && detail::has(target, key))
        throw GlanceError("merge conflict at " + path + '.' + key);
      target[key] = field.second;
    }
  }
}
} // namespace output_detail
/** Combine parts in order, with deep copies and stable layer positions. */
inline Json merge_parts(const std::vector<Json> &parts, MergeOptions options = {}) {
  Json result = Json::object({{"protocol", PROTOCOL_VERSION}});
  Json data, layers;
  for (const auto &input : parts) {
    auto part = input;
    if (!part.is_object())
      throw GlanceError("merge: part is not an object");
    if (detail::has(part, "content") || detail::has(part, "style"))
      throw GlanceError("merge: MatrixPart must be positioned with place()");
    if (detail::has(part, "protocol") && !detail::eq(part["protocol"], PROTOCOL_VERSION))
      throw GlanceError("merge: unsupported protocol");
    bool root = false;
    for (const auto *key : {"pixels", "default", "layout", "batch"})
      root = root || detail::has(part, key);
    if (root) {
      if (!detail::has(part, "pixels"))
        throw GlanceError("merge: root fields but no pixels");
      if (options.error_on_conflict && detail::has(result, "pixels"))
        throw GlanceError("merge conflict: root matrix");
      for (const auto *key : {"pixels", "default", "layout", "batch"}) {
        if (detail::has(part, key))
          result[key] = part[key];
        else
          options_detail::erase(result, key);
      }
    }
    if (detail::has(part, "data")) {
      if (!part["data"].is_object())
        throw GlanceError("merge: data must be an object");
      if (data.is_null())
        data = Json::object();
      output_detail::merge_data(data, part["data"], options.error_on_conflict, "data");
    }
    if (detail::has(part, "layers")) {
      if (!part["layers"].is_array())
        throw GlanceError("merge: layers must be an array");
      if (layers.is_null())
        layers = Json::array();
      for (const auto &layer : part["layers"].p->items) {
        auto &items = layers.p->items;
        auto found =
            std::find_if(items.begin(), items.end(), [&](const auto &v) { return detail::eq(v["id"], layer["id"]); });
        if (found == items.end())
          items.push_back(layer);
        else {
          if (options.error_on_conflict)
            throw GlanceError("merge conflict: duplicate layer id");
          *found = layer;
        }
      }
    }
  }
  if (!layers.is_null())
    result["layers"] = layers;
  if (!data.is_null())
    result["data"] = data;
  return result;
}
namespace output_detail {
/** Combine the Composites that make up one component. */
inline Json combine(const std::vector<Json> &parts) {
  auto merged = merge_parts(parts);
  return Json::object({{"layers", detail::has(merged, "layers") ? merged["layers"] : Json::array()},
                       {"data", detail::has(merged, "data") ? merged["data"] : Json::object()}});
}
} // namespace output_detail
inline Json merge_arguments(std::vector<Json> parts) {
  MergeOptions options;
  if (!parts.empty() && parts.back().is_object() && parts.back().size() == 1 &&
      detail::has(parts.back(), "onConflict")) {
    const auto &mode = parts.back()["onConflict"];
    options.error_on_conflict = mode.is_string() && mode.string() == "error";
    parts.pop_back();
  }
  return merge_parts(parts, options);
}
inline Json merge(const std::vector<Json> &parts, const MergeOptions &options) { return merge_parts(parts, options); }
template <class... Parts> inline Json merge(const Parts &...parts) { return merge_arguments({Json(parts)...}); }
/** Place one matrix part as a layer with independently owned data. */
inline Json place(const Json &part, const PlaceOptions &options) {
  Json node = Json::object({{"frame", frame_json(options.frame)}, {"content", part["content"]}, {"style", Json::object()}});
  if (options.z_index) node["zIndex"] = *options.z_index;
  if (options.rotation) node["rotation"] = *options.rotation;
  if (!options.transform.is_null()) node["transform"] = options.transform;
  return Json::object({{"layers", Json::array({Json::object({{"id", layer_id(options.id)}, {"type", "matrix"}, {"dataPath", options.path}})})}, {"data", nest_at_path(options.path, node)}});
}
inline Json place(const Json &part, const Json &options) {
  return place(part, PlaceOptions{options_detail::string(options, "id"), geometry_detail::frame(options["frame"]),
                                  options_detail::string(options, "path"), geometry_detail::number(options, "zIndex"),
                                  geometry_detail::number(options, "rotation"), options["transform"]});
}

/** Minimal invisible root for layer-only responses. */
inline Json empty_root() {
  auto empty = Json::object({{"empty", true}});
  return Json::object(
      {{"pixels", Json::array({Json::array({empty})})},
       {"default", empty},
       {"layout", Json::object({{"gapMode", "default"}, {"edgeInsetRatio", 0}, {"cellAspectRatio", 1}})}});
}
/** Finalize and validate a protocol response. */
inline Json to_response(const Json &part) {
  auto response = merge(part);
  if (!detail::has(response, "pixels"))
    response = merge(empty_root(), response);
  validate(response);
  return response;
}
} // namespace glance_widgets
