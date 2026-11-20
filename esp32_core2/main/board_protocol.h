#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
namespace core2 {
using Board = std::array<char, 64>;
inline bool supportedName(std::string name) {
  std::transform(name.begin(), name.end(), name.begin(),
                 [](unsigned char c) { return char(std::tolower(c)); });
  return name.find("chessnut") != std::string::npos ||
         name.find("smart chess") != std::string::npos ||
         name.find("millennium") != std::string::npos ||
         name.find("certabo") != std::string::npos ||
         name.find("spectrum") != std::string::npos ||
         name.find("cerno") != std::string::npos;
}
inline bool chessnutName(std::string name) {
  std::transform(name.begin(), name.end(), name.begin(),
                 [](unsigned char c) { return char(std::tolower(c)); });
  return name.find("chessnut") != std::string::npos ||
         name.find("smart chess") != std::string::npos;
}
inline bool certaboName(std::string name) {
  std::transform(name.begin(), name.end(), name.begin(),
                 [](unsigned char c) { return char(std::tolower(c)); });
  return name.find("certabo") != std::string::npos ||
         name.find("spectrum") != std::string::npos ||
         name.find("cerno") != std::string::npos;
}
inline int hex(uint8_t c) {
  c &= 127;
  return c >= '0' && c <= '9'   ? c - '0'
         : c >= 'A' && c <= 'F' ? c - 'A' + 10
         : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                : -1;
}
inline uint8_t parity(uint8_t c) {
  c &= 127;
  int ones = 0;
  for (uint8_t v = c; v; v >>= 1)
    ones += v & 1;
  return c | ((ones & 1) ? 0 : 128);
}
inline std::vector<uint8_t> modeCommand(const std::string &payload) {
  static const char *digits = "0123456789ABCDEF";
  uint8_t sum = 0;
  std::vector<uint8_t> out;
  for (uint8_t c : payload) {
    sum ^= c & 127;
    out.push_back(parity(c));
  }
  out.push_back(parity(digits[sum >> 4]));
  out.push_back(parity(digits[sum & 15]));
  return out;
}
inline std::vector<uint8_t> modePosition(const Board &board) {
  std::string payload = "s";
  for (int i=0;i<64;++i) payload += board[(i/8)*8+7-i%8];
  return modeCommand(payload);
}
inline bool chessnutFrame(const uint8_t *p, size_t len, Board &b) {
  if (len < 34 || p[0] != 1 || p[1] != 0x24)
    return false;
  static constexpr char pieces[] = ".qkbpnRPrBNQK";
  Board candidate{};
  for (int i = 0; i < 64; i++) {
    int code = (p[2 + i / 2] >> (4 * (i % 2))) & 15;
    if (code > 12)
      return false;
    candidate[63 - i] = pieces[code]; // H8..A1 -> canonical A1..H8
  }
  b = candidate;
  return true;
}
inline bool modeFrame(const uint8_t *p, size_t len, Board &b) {
  if (len != 67 || (p[0] & 127) != 's')
    return false;
  int h = hex(p[65]), l = hex(p[66]);
  if (h < 0 || l < 0)
    return false;
  uint8_t plain = 0, encoded = 0;
  for (int i = 0; i < 65; i++) {
    plain ^= p[i] & 127;
    encoded ^= parity(p[i]);
  }
  if ((h * 16 + l) != plain && (h * 16 + l) != encoded)
    return false;
  Board candidate{};
  for (int i = 0; i < 64; i++) {
    char c = p[1 + i] & 127;
    if (c == 0 || !std::strchr(".PNBRQKpnbrqk", c))
      return false;
    candidate[(i / 8) * 8 + 7 - i % 8] = c;
  }
  b = candidate;
  return true;
}
inline std::vector<uint8_t> chessnutLeds(uint64_t squares) {
  std::vector<uint8_t> p(10);
  p[0] = 10;
  p[1] = 8;
  for (int s = 0; s < 64; s++)
    if (squares & (1ULL << s))
      p[2 + 7 - s / 8] |= uint8_t(128 >> (s % 8));
  return p;
}
inline std::vector<uint8_t> modeLeds(uint64_t squares) {
  uint8_t grid[81] = {};
  for (int s = 0; s < 64; s++)
    if (squares & (1ULL << s)) {
      int f = 7 - s % 8, r = s / 8;
      grid[f * 9 + r] = grid[(f + 1) * 9 + r] = grid[f * 9 + r + 1] =
          grid[(f + 1) * 9 + r + 1] = 255;
    }
  std::string payload = "L01";
  static const char *digits = "0123456789ABCDEF";
  for (uint8_t value : grid) {
    payload += digits[value >> 4];
    payload += digits[value & 15];
  }
  return modeCommand(payload);
}
} // namespace core2
