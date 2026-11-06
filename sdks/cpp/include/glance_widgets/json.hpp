#pragma once
#include "number_format.hpp"
#include <cctype>
#include <cmath>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <memory>
#include <utility>
#include <vector>
namespace glance_widgets {
class GlanceValidationError : public GlanceError {
public:
  std::string path;
  GlanceValidationError(std::string location, const std::string &message)
      : GlanceError(location + ": " + message), path(std::move(location)) {}
};
namespace detail {
struct Value;
using Args = std::vector<Value>;
using Kw = std::vector<std::pair<std::string, Value>>;
enum class Kind { Null, Bool, Number, String, Array, Object };
struct Node;
/** Ordered JSON value with deep value semantics. */
struct Value {
  std::unique_ptr<Node> p;
  Value();
  ~Value();
  Value(const Value &);
  Value(Value &&);
  Value &operator=(const Value &);
  Value &operator=(Value &&);
  Value(std::nullptr_t);
  Value(bool);
  Value(double);
  Value(int n);
  Value(long n);
  Value(long long n);
  Value(unsigned n);
  Value(unsigned long n);
  Value(unsigned long long n);
  template <class T> Value(const std::vector<T> &items);
  Value(const char *s);
  Value(std::string s);
  static Value array(Args items = {});
  static Value object(Kw fields = {});
  bool is_null() const;
  bool is_bool() const;
  bool is_number() const;
  bool is_string() const;
  bool is_array() const;
  bool is_object() const;
  double number() const;
  std::string string() const;
  bool boolean() const;
  std::size_t size() const;
  Value &operator[](const std::string &key);
  const Value &operator[](const std::string &key) const;
  Value &operator[](int index);
  const Value &operator[](int index) const;
  std::string dump() const;
  std::string to_json() const { return dump(); }
  static Value parse(const std::string &text);
};
struct Node {
  Kind kind = Kind::Null;
  bool boolean = false;
  double number = 0;
  bool integer_source = false;
  std::string text;
  Args items;
  std::deque<std::pair<std::string, Value>> fields;
};
inline Value::Value() : p(std::make_unique<Node>()) {}
inline Value::~Value() = default;
inline Value::Value(const Value &other) : p(std::make_unique<Node>(*other.p)) {}
inline Value::Value(Value &&other) : Value() { p.swap(other.p); }
inline Value &Value::operator=(const Value &other) {
  if (this != &other)
    p = std::make_unique<Node>(*other.p);
  return *this;
}
inline Value &Value::operator=(Value &&other) {
  if (this != &other) {
    Value replacement(std::move(other));
    p.swap(replacement.p);
  }
  return *this;
}
template <class T> inline Value::Value(const std::vector<T> &items) : Value() {
  p->kind = Kind::Array;
  for (const auto &item : items)
    p->items.emplace_back(item);
}
inline Value::Value(std::nullptr_t) : Value() {}
inline Value::Value(bool n) : Value() {
  p->kind = Kind::Bool;
  p->boolean = n;
}
inline Value::Value(double n) : Value() {
  p->kind = Kind::Number;
  p->number = n;
}
inline Value::Value(int n) : Value(static_cast<double>(n)) { p->integer_source = true; }
inline Value::Value(long n) : Value(static_cast<double>(n)) { p->integer_source = true; }
inline Value::Value(long long n) : Value(static_cast<double>(n)) { p->integer_source = true; }
inline Value::Value(unsigned n) : Value(static_cast<double>(n)) { p->integer_source = true; }
inline Value::Value(unsigned long long n) : Value(static_cast<double>(n)) { p->integer_source = true; }
inline Value::Value(unsigned long n) : Value(static_cast<double>(n)) { p->integer_source = true; }
inline Value::Value(const char *s) : Value(std::string(s)) {}
inline Value::Value(std::string s) : Value() {
  p->kind = Kind::String;
  p->text = std::move(s);
}
inline bool Value::is_null() const { return p->kind == Kind::Null; }
inline bool Value::is_bool() const { return p->kind == Kind::Bool; }
inline bool Value::is_number() const { return p->kind == Kind::Number; }
inline bool Value::is_string() const { return p->kind == Kind::String; }
inline bool Value::is_array() const { return p->kind == Kind::Array; }
inline bool Value::is_object() const { return p->kind == Kind::Object; }
inline double Value::number() const {
  if (!is_number())
    throw GlanceError("Expected number");
  return p->number;
}
inline std::string Value::string() const {
  if (!is_string())
    throw GlanceError("Expected string");
  return p->text;
}
inline bool Value::boolean() const {
  if (!is_bool())
    throw GlanceError("Expected boolean");
  return p->boolean;
}
inline std::size_t Value::size() const {
  if (is_array())
    return p->items.size();
  if (is_object())
    return p->fields.size();
  if (is_string())
    return p->text.size();
  return 0;
}
inline Value Value::array(Args items) {
  Value v;
  v.p->kind = Kind::Array;
  v.p->items = std::move(items);
  return v;
}
inline Value Value::object(Kw fields) {
  Value v;
  v.p->kind = Kind::Object;
  for (auto &field : fields)
    v[field.first] = std::move(field.second);
  return v;
}
inline bool has(const Value &v, const std::string &key) {
  if (!v.is_object())
    return false;
  for (const auto &field : v.p->fields)
    if (field.first == key)
      return true;
  return false;
}
inline Value &Value::operator[](const std::string &key) {
  if (!is_object())
    throw GlanceError("Expected object");
  for (auto &field : p->fields)
    if (field.first == key)
      return field.second;
  p->fields.emplace_back(key, Value{});
  return p->fields.back().second;
}
inline const Value &Value::operator[](const std::string &key) const {
  for (const auto &field : p->fields)
    if (field.first == key)
      return field.second;
  static const Value missing;
  return missing;
}
inline Value &Value::operator[](int index) {
  if (!is_array() || index < 0 || static_cast<std::size_t>(index) >= size())
    throw GlanceError("Array index out of bounds");
  return p->items[static_cast<std::size_t>(index)];
}
inline const Value &Value::operator[](int index) const {
  if (!is_array() || index < 0 || static_cast<std::size_t>(index) >= size())
    throw GlanceError("Array index out of bounds");
  return p->items[static_cast<std::size_t>(index)];
}
inline bool eq(const Value &a, const Value &b) {
  if (a.p->kind != b.p->kind)
    return false;
  if (a.is_null())
    return true;
  if (a.is_number())
    return a.number() == b.number();
  if (a.is_bool())
    return a.boolean() == b.boolean();
  if (a.is_string())
    return a.string() == b.string();
  if (a.size() != b.size())
    return false;
  if (a.is_array()) {
    for (std::size_t i = 0; i < a.size(); ++i)
      if (!eq(a.p->items[i], b.p->items[i]))
        return false;
  } else {
    for (const auto &field : a.p->fields)
      if (!has(b, field.first) || !eq(field.second, b[field.first]))
        return false;
  }
  return true;
}
inline Value get(const Value &v, const std::string &key) { return v[key]; }
inline const Args &iter(const Value &v) {
  if (!v.is_array())
    throw GlanceError("Expected array");
  return v.p->items;
}
inline bool truth(const Value &v) {
  if (v.is_null())
    return false;
  if (v.is_bool())
    return v.boolean();
  if (v.is_number())
    return v.number() != 0 && !std::isnan(v.number());
  if (v.is_string())
    return !v.string().empty();
  return true;
}
inline std::string quote(const std::string &s) {
  std::string out = "\"";
  const char *hex = "0123456789abcdef";
  for (std::size_t i = 0; i < s.size(); ++i) {
    const auto c = static_cast<unsigned char>(s[i]);
    if (c == 0xed && i + 2 < s.size()) {
      const auto b = static_cast<unsigned char>(s[i + 1]);
      const auto d = static_cast<unsigned char>(s[i + 2]);
      if (b >= 0xa0 && b <= 0xbf && (d & 0xc0) == 0x80) {
        const unsigned cp = ((c & 15u) << 12) | ((b & 63u) << 6) | (d & 63u);
        out += "\\u";
        for (int shift = 12; shift >= 0; shift -= 4)
          out += hex[(cp >> shift) & 15u];
        i += 2;
        continue;
      }
    }
    if (c == '"' || c == '\\') {
      out += '\\';
      out += char(c);
    } else if (c == '\b')
      out += "\\b";
    else if (c == '\f')
      out += "\\f";
    else if (c == '\n')
      out += "\\n";
    else if (c == '\r')
      out += "\\r";
    else if (c == '\t')
      out += "\\t";
    else if (c < 32) {
      out += "\\u00";
      out += hex[c >> 4];
      out += hex[c & 15];
    } else
      out += char(c);
  }
  return out + '"';
}
inline std::string utf8(unsigned cp) {
  std::string s;
  if (cp < 128)
    s += char(cp);
  else if (cp < 2048) {
    s += char(192 | (cp >> 6));
    s += char(128 | (cp & 63));
  } else if (cp < 65536) {
    s += char(224 | (cp >> 12));
    s += char(128 | ((cp >> 6) & 63));
    s += char(128 | (cp & 63));
  } else {
    s += char(240 | (cp >> 18));
    s += char(128 | ((cp >> 12) & 63));
    s += char(128 | ((cp >> 6) & 63));
    s += char(128 | (cp & 63));
  }
  return s;
}
struct Parser {
  const std::string &s;
  std::size_t p = 0;
  int depth = 0;
  struct Depth {
    int &d;
    explicit Depth(int &value) : d(value) {
      if (++d > 512)
        throw GlanceError("JSON nesting too deep");
    }
    ~Depth() { --d; }
  };
  void ws() {
    while (p < s.size() && (s[p] == ' ' || s[p] == '\t' || s[p] == '\n' || s[p] == '\r'))
      p++;
  }
  char take() {
    if (p >= s.size())
      throw GlanceError("Unexpected end of JSON");
    return s[p++];
  }
  unsigned hex4() {
    unsigned n = 0;
    for (int i = 0; i < 4; i++) {
      char c = take();
      n <<= 4;
      if (c >= '0' && c <= '9')
        n += static_cast<unsigned>(c - '0');
      else if (c >= 'a' && c <= 'f')
        n += static_cast<unsigned>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F')
        n += static_cast<unsigned>(c - 'A' + 10);
      else
        throw GlanceError("Invalid JSON escape");
    }
    return n;
  }
  std::string text() {
    if (take() != '"')
      throw GlanceError("Expected string");
    std::string o;
    while (true) {
      char c = take();
      if (c == '"')
        return o;
      if (static_cast<unsigned char>(c) < 32)
        throw GlanceError("Invalid string");
      if (c != '\\') {
        o += c;
        continue;
      }
      char e = take();
      if (e == '"' || e == '\\' || e == '/')
        o += e;
      else if (e == 'b')
        o += '\b';
      else if (e == 'f')
        o += '\f';
      else if (e == 'n')
        o += '\n';
      else if (e == 'r')
        o += '\r';
      else if (e == 't')
        o += '\t';
      else if (e == 'u') {
        unsigned c1 = hex4();
        // Preserve lone UTF-16 surrogates internally as WTF-8. Serialization
        // escapes them again, matching well-formed JSON.stringify output.
        if (c1 >= 0xd800 && c1 <= 0xdbff && p + 6 <= s.size() && s[p] == '\\' && s[p + 1] == 'u') {
          const auto saved = p;
          p += 2;
          const unsigned c2 = hex4();
          if (c2 >= 0xdc00 && c2 <= 0xdfff)
            c1 = 0x10000 + ((c1 - 0xd800) << 10) + (c2 - 0xdc00);
          else
            p = saved;
        }
        o += utf8(c1);
      } else
        throw GlanceError("Invalid escape");
    }
  }
  Value read() {
    Depth guard(depth);
    ws();
    char c = p < s.size() ? s[p] : 0;
    if (c == '"')
      return text();
    if (c == '[') {
      p++;
      Args a;
      ws();
      if (p < s.size() && s[p] == ']') {
        p++;
        return Value::array();
      }
      while (true) {
        a.push_back(read());
        ws();
        char d = take();
        if (d == ']')
          break;
        if (d != ',')
          throw GlanceError("Expected comma");
      }
      return Value::array(a);
    }
    if (c == '{') {
      p++;
      Kw o;
      ws();
      if (p < s.size() && s[p] == '}') {
        p++;
        return Value::object();
      }
      while (true) {
        ws();
        auto k = text();
        ws();
        if (take() != ':')
          throw GlanceError("Expected colon");
        o.emplace_back(k, read());
        ws();
        char d = take();
        if (d == '}')
          break;
        if (d != ',')
          throw GlanceError("Expected comma");
      }
      return Value::object(o);
    }
    for (auto token : {std::string("true"), std::string("false"), std::string("null")}) {
      if (s.compare(p, token.size(), token) == 0) {
        p += token.size();
        return token == "null" ? Value() : Value(token == "true");
      }
    }
    std::size_t start = p;
    if (c == '-')
      p++;
    if (p >= s.size() || !std::isdigit(static_cast<unsigned char>(s[p])))
      throw GlanceError("Invalid JSON token");
    if (s[p] == '0')
      p++;
    else
      while (p < s.size() && std::isdigit(static_cast<unsigned char>(s[p])))
        p++;
    if (p < s.size() && s[p] == '.') {
      p++;
      auto q = p;
      while (p < s.size() && std::isdigit(static_cast<unsigned char>(s[p])))
        p++;
      if (q == p)
        throw GlanceError("Invalid JSON number");
    }
    if (p < s.size() && (s[p] == 'e' || s[p] == 'E')) {
      p++;
      if (p < s.size() && (s[p] == '+' || s[p] == '-'))
        p++;
      auto q = p;
      while (p < s.size() && std::isdigit(static_cast<unsigned char>(s[p])))
        p++;
      if (q == p)
        throw GlanceError("Invalid exponent");
    }
    // JSON syntax has already been checked. strtod preserves subnormals and
    // returns infinity/zero for range overflow/underflow without throwing.
    const auto token = s.substr(start, p - start);
    char *end = nullptr;
    const double value = std::strtod(token.c_str(), &end);
    if (end != token.c_str() + token.size())
      throw GlanceError("Invalid JSON number");
    Value result(value);
    result.p->integer_source = token.find_first_of(".eE") == std::string::npos;
    return result;
  }
};
inline Value Value::parse(const std::string &s) {
  Parser p{s};
  Value v = p.read();
  p.ws();
  if (p.p != s.size())
    throw GlanceError("Trailing JSON");
  return v;
}
inline std::string Value::dump() const {
  if (is_null())
    return "null";
  if (is_bool())
    return boolean() ? "true" : "false";
  if (is_number())
    return std::isfinite(number()) ? numstr(number()) : "null";
  if (is_string())
    return quote(string());
  std::string out = is_array() ? "[" : "{";
  if (is_array()) {
    for (const auto &item : p->items) {
      if (out.size() > 1)
        out += ',';
      out += item.dump();
    }
  } else {
    for (const auto &field : p->fields) {
      if (out.size() > 1)
        out += ',';
      out += quote(field.first) + ':' + field.second.dump();
    }
  }
  return out + (is_array() ? "]" : "}");
}
/** Decode UTF-8 by scalar value; replace malformed bytes deterministically. */
inline std::vector<std::string> chars(const std::string &s) {
  std::vector<std::string> out;
  for (std::size_t i = 0; i < s.size();) {
    unsigned c = static_cast<unsigned char>(s[i]);
    std::size_t n = c < 128 ? 1 : c >= 194 && c <= 223 ? 2 : c <= 239 && c >= 224 ? 3 : c >= 240 && c <= 244 ? 4 : 0;
    unsigned cp = n == 1 ? c : c & (n == 2 ? 31u : n == 3 ? 15u : 7u);
    bool valid = n && i + n <= s.size();
    for (std::size_t j = 1; valid && j < n; ++j) {
      unsigned b = static_cast<unsigned char>(s[i + j]);
      valid = (b & 192u) == 128u;
      cp = (cp << 6) | (b & 63u);
    }
    valid = valid && (n != 2 || cp >= 128) && (n != 3 || cp >= 2048) && (n != 4 || (cp >= 65536 && cp <= 0x10ffff));
    out.push_back(valid ? s.substr(i, n) : utf8(0xfffd));
    i += valid ? n : 1;
  }
  return out;
}
} // namespace detail
using Value = detail::Value;
using Json = Value;
} // namespace glance_widgets
