#pragma once
#include <cstddef>
#include <string>
#include <utility>
#include <vector>
namespace glance_widgets {
inline constexpr int PROTOCOL_VERSION = 1;
inline constexpr std::size_t MAX_RESPONSE_BYTES = 524288, MAX_MATRIX_CELLS = 4096;
inline constexpr const char *LAYER_ID_PREFIX = "remote:";
inline constexpr double MAX_SAFE_NATIVE_INTEGER = 9.2e18;
inline constexpr double CELL_ASPECT_RATIO_MIN = 0.1, CELL_ASPECT_RATIO_MAX = 10;
inline constexpr double EDGE_INSET_RATIO_LIMIT = 0.45, GAP_RATIO_MAX = 10;
inline constexpr double FRAME_SIZE_MIN = 0.02, FRAME_SIZE_MAX = 1.5, SHAPE_MIN_SIZE_PT = 24;
inline constexpr const char *DATA_PATH_PATTERN = R"(^data(?:\.[A-Za-z_][A-Za-z0-9_]*|\[[0-9]+\])*$)";
inline const std::vector<std::pair<std::string, double>> GAP_MODE_RATIOS{
    {"zero", 0}, {"near", 0.08}, {"default", 0.16}, {"far", 0.28}, {"extraFar", 0.42}};
namespace path_detail {
inline bool ident_start(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; }
inline bool digit(char c) { return c >= '0' && c <= '9'; }
/// Identifier [A-Za-z_][A-Za-z0-9_]*, whole string.
inline bool is_identifier(const std::string &s) {
  if (s.empty() || !ident_start(s[0]))
    return false;
  for (char c : s)
    if (!ident_start(c) && !digit(c))
      return false;
  return true;
}
/// Scan `data` followed by `.ident` (and `[digits]` when allow_index) segments. Segments are
/// returned as (is_index, text). Equivalent to DATA_PATH_PATTERN, anchored on the whole string.
inline bool scan_data_path(const std::string &path, bool allow_index, bool require_segment,
                           std::vector<std::pair<bool, std::string>> *segments = nullptr) {
  if (path.compare(0, 4, "data") != 0)
    return false;
  std::size_t i = 4, count = 0;
  while (i < path.size()) {
    if (path[i] == '.' && i + 1 < path.size() && ident_start(path[i + 1])) {
      std::size_t j = i + 1;
      while (j < path.size() && (ident_start(path[j]) || digit(path[j])))
        ++j;
      if (segments)
        segments->emplace_back(false, path.substr(i + 1, j - i - 1));
      i = j;
    } else if (allow_index && path[i] == '[') {
      std::size_t j = i + 1;
      while (j < path.size() && digit(path[j]))
        ++j;
      if (j == i + 1 || j >= path.size() || path[j] != ']')
        return false;
      if (segments)
        segments->emplace_back(true, path.substr(i + 1, j - i - 1));
      i = j + 1;
    } else
      return false;
    ++count;
  }
  return !require_segment || count > 0;
}
inline bool is_data_path(const std::string &path) { return scan_data_path(path, true, false); }
} // namespace path_detail
} // namespace glance_widgets
