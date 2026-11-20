#pragma once
#include "types.h"
#include <cstdlib>
#include <sstream>
#include <string>
namespace core2 {
struct QueenMood {
  int playerEval = 0;
  int after = 0;
  bool haveAfter = false;
  static int stage(int cp) {
    if (cp >= 250) return 0;
    if (cp >= 120) return 1;
    if (cp > -120) return 2;
    if (cp > -250) return 3;
    if (cp > -400) return 4;
    return 5;
  }
  void reset() { playerEval = after = 0; haveAfter = false; }
  void beginSearch() { after = 0; haveAfter = false; }
  void flip() { playerEval = -playerEval; beginSearch(); }
  bool readAdaptive(const std::string &line) {
    if (line.rfind("info string adaptive ", 0) != 0) return false;
    std::istringstream input(line);
    std::string token;
    while (input >> token) {
      if (token == "after") {
        int value;
        if (!(input >> value)) return false;
        after = value;
        haveAfter = true;
        return true;
      }
    }
    return false;
  }
  void finishSearch(int engineScore) {
    int value = engineScore;
    if (is_mate_score(value)) {
      int distance = (SCORE_MATE - std::abs(value) + 1) / 2;
      value = value > 0 ? 100000 - distance : -100000 + distance;
    }
    playerEval = haveAfter ? after : -value;
  }
};
} // namespace core2
