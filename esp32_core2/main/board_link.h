#pragma once
#include "board_protocol.h"
#include <array>
#include <cstdint>
#include <string>
namespace core2 {
struct Peer {
  std::string name, address;
  uint8_t type = 0;
  int rssi = 0;
  bool chessnut = false, certabo = false;
};
struct LinkView {
  bool scanning = false, connecting = false, connected = false,
       haveBoard = false;
  bool certabo = false, spectrum = false, calibrating = false, virtualPosition = false;
  unsigned revision = 0;
  std::string name, status = "Nicht verbunden";
  Board board{};
  std::array<Peer, 5> peers{};
  unsigned count = 0;
};
void linkStart();
bool linkStartingPosition();
LinkView linkView();
void linkScan();
void linkChoose(unsigned index);
void linkLeds(uint64_t mask, int playerEval = 0);
} // namespace core2
