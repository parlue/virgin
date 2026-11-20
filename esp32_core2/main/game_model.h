#pragma once
#include "board_protocol.h"
#include "movegen.h"
#include "position.h"
#include "queen_mood.h"
#include <string>
#include <vector>
namespace core2 {
class Game {
public:
  Position pos;
  QueenMood mood;
  Color human = WHITE;
  unsigned epoch = 0;
  bool setup = true, thinking = false, ended = false, rotated = false,
       havePhysical = false;
  bool atStart = false;
  Move pending = 0, last = 0;
  Board physical{};
  std::string message = "Brett verbinden";
  std::vector<std::string> moves;
  std::vector<Key> history;
  static constexpr const char *startFen =
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  static Board diagram(const Position &p) {
    Board b{};
    const char *codes = "PNBRQKpnbrqk";
    for (int s = 0; s < 64; s++) {
      Piece piece = p.pieceOn(Square(s));
      b[s] = piece == NO_PIECE ? '.' : codes[int(piece)];
    }
    return b;
  }
  void start(Color player) {
    ++epoch;
    mood.reset();
    human = player;
    pos.setFen(startFen);
    setup = true;
    thinking = false;
    ended = false;
    pending = last = 0;
    rotated = false;
    havePhysical = false;
    atStart = false;
    moves.clear();
    history = {pos.key()};
    physical = diagram(pos);
    message = "Grundstellung aufbauen";
  }
  Board expected() const {
    Position p = pos;
    if (pending)
      p.applyMove(pending);
    return diagram(p);
  }
  // Preview the complete engine move immediately, including
  // rook/capture/promotion. The physical board only confirms execution; it must
  // not drive this preview.
  Board displayBoard() const {
    return pending ? expected() : (havePhysical ? physical : diagram(pos));
  }
  uint64_t differences() const {
    if (!havePhysical)
      return 0;
    Board target = expected();
    uint64_t mask = 0;
    for (int i = 0; i < 64; i++)
      if (target[i] != physical[i])
        mask |= 1ULL << i;
    return mask;
  }
  uint64_t ledSquares() const {
    uint64_t mask = differences();
    if (pending)
      mask |= (1ULL << from_sq(pending)) | (1ULL << to_sq(pending));
    if (rotated) {
      uint64_t rev = 0;
      for (int i = 0; i < 64; i++)
        if (mask & (1ULL << i))
          rev |= 1ULL << (63 - i);
      return rev;
    }
    return mask;
  }
  bool needsEngine() const {
    return !setup && !ended && !thinking && !pending &&
           pos.sideToMove() != human;
  }
  bool forceMove() {
    if (setup || ended || thinking || pending || !havePhysical ||
        physical != diagram(pos) || pos.sideToMove() != human)
      return false;
    human = ~pos.sideToMove();
    mood.flip();
    message = "Virgin ist am Zug";
    return true;
  }
  void finishCheck() {
    MoveList legal;
    generateLegal(pos, legal);
    if (!legal.count) {
      ended = true;
      mood.playerEval = pos.inCheck() ? (pos.sideToMove() == human ? -100000 : 100000) : 0;
      message = pos.inCheck()
                    ? (pos.sideToMove() == human ? "Matt - Virgin gewinnt"
                                                 : "Matt - du gewinnst")
                    : "Remis - Patt";
      return;
    }
    if (!(pos.pieces(PAWN) | pos.pieces(ROOK) | pos.pieces(QUEEN))) {
      const Bitboard bishops = pos.pieces(BISHOP);
      const Bitboard knights = pos.pieces(KNIGHT);
      const bool dead =
          popcount(bishops | knights) <= 1 ||
          (!knights && (!(bishops & DarkSquares) || !(bishops & LightSquares)));
      if (dead) {
        ended = true;
        mood.playerEval = 0;
        message = "Remis - Material";
        return;
      }
    }
    int repetitions = 0;
    for (Key k : history)
      repetitions += k == pos.key();
    if (repetitions >= 3 || pos.halfmoveClock() >= 100) {
      ended = true;
      mood.playerEval = 0;
      message = "Remis";
      return;
    }
    message =
        pos.sideToMove() == human ? "Du bist am Zug" : "Virgin ist am Zug";
  }
  void commit(Move m) {
    moves.push_back(move_to_uci(m));
    pos.applyMove(m);
    history.push_back(pos.key());
    last = m;
    pending = 0;
    finishCheck();
  }
  void board(const Board &raw) {
    Position initial;
    initial.setFen(startFen);
    Board initialBoard = diagram(initial), reversed = initialBoard;
    std::reverse(reversed.begin(), reversed.end());
    const bool isStart = raw == initialBoard || raw == reversed;
    // Trigger on entry, not on duplicate notifications while Virgin opens.
    if (isStart && !atStart && !setup)
      start(WHITE);
    atStart = isStart;
    Board b = raw;
    if (setup) {
      Board normal = diagram(pos), reverse = normal;
      std::reverse(reverse.begin(), reverse.end());
      if (raw == normal) {
        rotated = false;
        setup = false;
      } else if (raw == reverse) {
        rotated = true;
        setup = false;
      }
      if (rotated)
        std::reverse(b.begin(), b.end());
      physical = b;
      havePhysical = true;
      if (!setup)
        finishCheck();
      return;
    }
    if (rotated)
      std::reverse(b.begin(), b.end());
    physical = b;
    havePhysical = true;
    if (ended)
      return;
    if (pending) {
      if (b == expected())
        commit(pending);
      else
        message = "Virgin-Zug ausfuehren";
      return;
    }
    if (b == diagram(pos)) {
      if (!thinking)
        finishCheck();
      return;
    }
    if (thinking || pos.sideToMove() != human) {
      message = "Figuren zurueckstellen";
      return;
    }
    MoveList legal;
    generateLegal(pos, legal);
    for (Move m : legal) {
      Position next = pos;
      next.applyMove(m);
      if (diagram(next) == b) {
        commit(m);
        return;
      }
    }
    message = "Zug vollstaendig ausfuehren";
  }
  bool engineResult(const std::string &text, unsigned generation) {
    if (generation != epoch || !thinking)
      return false;
    thinking = false;
    Move m = find_legal_move(pos, text);
    if (!m) {
      message = "Engine-Antwort ungueltig";
      return false;
    }
    pending = m;
    message = "Virgin-Zug ausfuehren";
    return true;
  }
  std::string positionCommand() const {
    std::string out = "position startpos";
    if (!moves.empty())
      out += " moves";
    for (const auto &move : moves)
      out += ' ' + move;
    return out;
  }
};
} // namespace core2
