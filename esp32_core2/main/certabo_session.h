#pragma once
#include "board_protocol.h"
#include "certabo_protocol.h"
#include "certabo_led_identity.h"
#include "certabo_startup.h"
#include "certabo_spectrum_leds.h"
#include <algorithm>
#include <array>
#include <vector>

namespace core2 {
// Same player-perspective colour stops as chesstactics talLedColour().
inline std::array<uint8_t, 3> spectrumMoodColour(int playerEval) {
  constexpr int stops[5][3] = {{140,0,0}, {240,90,10}, {70,180,255},
                              {40,190,150}, {46,204,113}};
  const int cp = std::clamp(playerEval, -400, 400);
  const int offset = cp + 400;
  const int segment = std::min(offset / 200, 3);
  const int part = offset - segment * 200;
  std::array<uint8_t, 3> rgb{};
  for (int i = 0; i < 3; ++i)
    rgb[i] = uint8_t((stops[segment][i] * (200-part) +
                      stops[segment+1][i] * part) / 200);
  return rgb;
}
// chesstactics CertaboProtocol.spectrumTint(): light blue otherwise appears
// almost white on this hardware. Leave non-blue-dominant colours untouched.
inline std::array<uint8_t,3> spectrumTint(std::array<uint8_t,3> rgb) {
  const float t = std::clamp(float(int(rgb[2])-int(rgb[1]))/60.0f,0.0f,1.0f);
  rgb[0] = uint8_t(float(rgb[0])*(1.0f-t*0.9f));
  rgb[1] = uint8_t(float(rgb[1])*(1.0f-t*0.8f));
  return rgb;
}

class CertaboSession {
 public:
  void reset() {
    parser_.reset(); calibration_.reset(); identity_.reset(); startup_.reset();
    raw_ = {}; haveRaw_ = false;
  }
  void gap() {
    parser_.resetStream(); calibration_.resetWindow();
    identity_.resetSetupWindow(); startup_.resetSetupWindow();
  }
  bool feed(uint8_t byte, Board &out) {
    const auto result = parser_.feed(byte);
    if (result == certabo::ParseResult::Invalid) {
      calibration_.resetWindow(); identity_.resetSetupWindow();
      startup_.resetSetupWindow();
    }
    if (result != certabo::ParseResult::Position) return false;
    const auto &raw = parser_.board();
    if (!calibration_.ready()) {
      raw_ = raw; haveRaw_ = true;
      const auto previous = identity_.type();
      identity_.observeSetup(raw);
      if (previous != identity_.type()) calibration_.resetWindow();
      if (identity_.type() == certabo::LedType::Unknown) {
        calibration_.resetWindow();
        return false;
      }
      startup_.observeSetup(raw, identity_.type());
      if (identity_.conflictingSetup(raw)) {
        calibration_.resetWindow();
        return false;
      }
      if (!calibration_.observe(raw, true, true)) return false;
      if (calibration_.setupType() != identity_.type()) {
        calibration_.reset();
        return false;
      }
      startup_.learned();
    }
    uint8_t frame[67]{};
    if (!calibration_.decode(raw, frame)) return false;
    raw_ = raw; haveRaw_ = true;
    if (startup_.active() && !startup_.finish(frame)) return false;
    // Gateway mode-B frame h1..a8 -> canonical a1..h8.
    for (int i = 0; i < 64; ++i) out[i] = char(frame[64-(i^56)]);
    return true;
  }
  certabo::LedType type() const { return identity_.type(); }
  bool spectrum() const { return identity_.spectrum(); }
  bool calibrated() const { return calibration_.ready(); }
  bool ready() const { return calibrated() && !startup_.active(); }
  std::vector<uint8_t> clearLeds() const {
    if (type() == certabo::LedType::Unknown) return {};
    if (!spectrum()) return std::vector<uint8_t>(8,0);
    uint8_t mask[8]{};
    std::vector<uint8_t> command(certabo::spectrum::kCommandBytes);
    certabo::spectrum::encode(mask,command.data(),0,0,0);
    return command;
  }
  const char *status() const {
    if (ready()) return "Brett verbunden";
    if (calibrated()) return "Zusatzdamen weg";
    if (type() == certabo::LedType::Unknown) return "Damen: d3/d6=C e3/e6=Spectrum";
    return spectrum() ? "Spectrum aufbauen" : "Certabo aufbauen";
  }
  std::vector<uint8_t> leds(uint64_t squares, int playerEval, uint32_t now) {
    if (!haveRaw_ || type() == certabo::LedType::Unknown) return {};
    uint8_t mask[8]{};
    if (startup_.active())
      startup_.mask(now, raw_, mask, calibrated() ? &calibration_ : nullptr, false);
    else {
      uint8_t indices[64]; size_t count = 0;
      for (uint8_t i = 0; i < 64; ++i)
        if (squares & (uint64_t(1) << i)) indices[count++] = i;
      certabo::ledMask(indices, count, mask);
    }
    if (!spectrum()) return {mask, mask+8};
    const auto rgb = startup_.active() ? std::array<uint8_t,3>{0,0,204}
                                       : spectrumTint(spectrumMoodColour(playerEval));
    std::vector<uint8_t> command(certabo::spectrum::kCommandBytes);
    certabo::spectrum::encode(mask, command.data(), rgb[0], rgb[1], rgb[2]);
    return command;
  }
  void sent(uint32_t now) { startup_.sent(now); }
 private:
  certabo::Parser parser_;
  certabo::Calibration calibration_;
  certabo::LedIdentity identity_;
  certabo::Startup startup_;
  certabo::RawBoard raw_{};
  bool haveRaw_ = false;
};
} // namespace core2
