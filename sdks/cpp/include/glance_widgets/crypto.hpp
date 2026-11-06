#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace glance_widgets {
namespace detail {
inline std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
inline std::string sha256(std::string s) {
  static constexpr std::uint32_t k[] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
      0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
      0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
      0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  std::uint64_t bits = s.size() * 8;
  s += '\x80';
  while (s.size() % 64 != 56)
    s += '\0';
  for (int i = 7; i >= 0; i--)
    s += char(bits >> (i * 8));
  std::array<std::uint32_t, 8> h{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  for (std::size_t p = 0; p < s.size(); p += 64) {
    std::uint32_t w[64]{};
    for (int i = 0; i < 16; i++)
      for (int j = 0; j < 4; j++)
        w[i] = (w[i] << 8) | static_cast<unsigned char>(s[p + static_cast<std::size_t>(i * 4 + j)]);
    for (int i = 16; i < 64; i++)
      w[i] = w[i - 16] + (rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3)) + w[i - 7] +
             (rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10));
    auto a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], z = h[7];
    for (int i = 0; i < 64; i++) {
      auto t1 = z + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ ((~e) & g)) + k[i] + w[i];
      auto t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
      z = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    std::uint32_t v[]{a, b, c, d, e, f, g, z};
    for (int i = 0; i < 8; i++)
      h[static_cast<std::size_t>(i)] += v[i];
  }
  std::string out;
  for (auto x : h)
    for (int i = 3; i >= 0; i--)
      out += char(x >> (i * 8));
  return out;
}
inline std::string b64(const std::string &s) {
  static const char *c = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  for (std::size_t i = 0; i < s.size(); i += 3) {
    unsigned a = static_cast<unsigned char>(s[i]), b = i + 1 < s.size() ? static_cast<unsigned char>(s[i + 1]) : 0,
             d = i + 2 < s.size() ? static_cast<unsigned char>(s[i + 2]) : 0;
    unsigned n = (a << 16) | (b << 8) | d;
    out += c[n >> 18];
    out += c[(n >> 12) & 63];
    out += i + 1 < s.size() ? c[(n >> 6) & 63] : '=';
    out += i + 2 < s.size() ? c[n & 63] : '=';
  }
  return out;
}
inline std::string signature(const std::string &timestamp, std::string key) {
  std::string message = timestamp + "\n" + key;
  if (key.size() > 64)
    key = sha256(key);
  key.resize(64, '\0');
  std::string inner(64, '\0'), outer(64, '\0');
  for (int i = 0; i < 64; i++) {
    inner[static_cast<std::size_t>(i)] = key[static_cast<std::size_t>(i)] ^ 0x36;
    outer[static_cast<std::size_t>(i)] = key[static_cast<std::size_t>(i)] ^ 0x5c;
  }
  return b64(sha256(outer + sha256(inner + message)));
}
inline bool constant_equal(const std::string &a, const std::string &b) {
  if (a.size() != b.size())
    return false;
  unsigned d = 0;
  for (std::size_t i = 0; i < a.size(); i++)
    d |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
  return d == 0;
}
} // namespace detail
} // namespace glance_widgets
