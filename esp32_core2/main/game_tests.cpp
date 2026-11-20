#include "bitboard.h"
#include "game_model.h"
#include "position.h"
#include <iostream>
#include <stdexcept>
using namespace core2;
static int passed = 0;
void check(bool ok, const char *what) {
  if (!ok)
    throw std::runtime_error(what);
  ++passed;
  std::cout << "PASS " << what << '\n';
}
void set(Game &g, const char *fen) {
  g.start(WHITE);
  g.pos.setFen(fen);
  g.setup = false;
  g.physical = Game::diagram(g.pos);
  g.havePhysical = true;
  g.history = {g.pos.key()};
}
Board moveBoard(const Position &p, const char *uci) {
  Position next = p;
  auto m = find_legal_move(next, uci);
  if (!m)
    throw std::runtime_error(uci);
  next.applyMove(m);
  return Game::diagram(next);
}
int main() {
  try {
    init_bitboards();
    init_zobrist();
    check(supportedName("Chessnut Air 002A") && supportedName("CHESSNUT GO") &&
              supportedName("Chessnut Pro") &&
              supportedName("MILLENNIUM CHESS") && !supportedName("speaker"),
          "model suffixes do not restrict board discovery");
    Game g;
    g.start(WHITE);
    Board start = Game::diagram(g.pos);
    g.board(start);
    check(!g.setup && !g.needsEngine(), "white starts after physical setup");
    Board partial = start;
    partial[SQ_E2] = '.';
    g.board(partial);
    check(g.moves.empty(), "lifted pawn is not committed as a move");
    g.board(moveBoard(g.pos, "e2e4"));
    check(g.moves.size() == 1 && g.needsEngine(),
          "completed human move requests the engine");
    g.thinking = true;
    check(g.engineResult("e7e5", g.epoch), "legal engine move is accepted");
    check(g.moves.size() == 1 && g.pending &&
              g.ledSquares() == ((1ULL << SQ_E7) | (1ULL << SQ_E5)),
          "engine waits for physical execution and lights source/destination");
    Board target = g.expected();
    check(g.displayBoard() == target && g.physical != target &&
              g.pos.pieceOn(SQ_E7) == make_piece(BLACK, PAWN),
          "screen leads physical board without prematurely committing engine "
          "move");
    partial = g.physical;
    partial[SQ_E7] = '.';
    g.board(partial);
    check(g.displayBoard() == target && g.pending && g.moves.size() == 1,
          "lifting engine piece cannot undo or duplicate the screen preview");
    g.board(target);
    check(g.moves.size() == 2 && !g.pending && g.pos.sideToMove() == WHITE,
          "physical reply confirmation commits exactly once");
    g.board(target);
    check(g.moves.size() == 2,
          "duplicate board reports cannot duplicate moves");
    unsigned old = g.epoch;
    g.thinking = true;
    g.start(BLACK);
    check(!g.engineResult("e2e4", old) && !g.pending,
          "old search results cannot leak into a new game");
    g.board(start);
    check(g.needsEngine(), "choosing black lets Virgin move first");
    g.start(BLACK);
    Board reversed = start;
    std::reverse(reversed.begin(), reversed.end());
    g.board(reversed);
    check(!g.setup && g.rotated && g.physical == start,
          "physical board can be rotated without changing chess coordinates");
    set(g, "k7/8/8/8/8/8/8/4K2R w K - 0 1");
    partial = Game::diagram(g.pos);
    partial[SQ_E1] = '.';
    partial[SQ_G1] = 'K';
    g.board(partial);
    check(g.moves.empty(), "castling waits for the rook as well as the king");
    g.board(moveBoard(g.pos, "e1g1"));
    check(g.moves.size() == 1 &&
              g.pos.pieceOn(SQ_F1) == make_piece(WHITE, ROOK),
          "complete castling uses the legal move flags");
    set(g, "7k/8/8/3pP3/8/8/8/K7 w - d6 0 1");
    partial = Game::diagram(g.pos);
    partial[SQ_E5] = '.';
    partial[SQ_D6] = 'P';
    g.board(partial);
    check(g.moves.empty(), "en passant waits for removal of the captured pawn");
    g.board(moveBoard(g.pos, "e5d6"));
    check(g.moves.size() == 1 && g.pos.pieceOn(SQ_D5) == NO_PIECE,
          "en passant commits the full legal board transition");
    set(g, "7k/P7/8/8/8/8/8/7K w - - 0 1");
    g.board(moveBoard(g.pos, "a7a8n"));
    check(g.moves.size() == 1 &&
              g.pos.pieceOn(SQ_A8) == make_piece(WHITE, KNIGHT),
          "physical underpromotion preserves the selected piece");
    // A complete starting arrangement is an edge-triggered new game.
    g.start(WHITE);
    g.board(start);
    check(g.forceMove() && g.human == BLACK && g.needsEngine(),
          "Move hands white to Virgin and switches the human to black");
    unsigned openingEpoch = g.epoch;
    g.thinking = true;
    g.board(start);
    check(g.epoch == openingEpoch && g.thinking && !g.forceMove(),
          "duplicate starting board cannot reset an opening search");
    g.engineResult("e2e4", openingEpoch);
    g.board(start);
    check(g.pending && g.epoch == openingEpoch && !g.forceMove(),
          "duplicate starting board preserves pending opening and blocks Move");
    g.board(g.expected());
    check(g.moves.size() == 1 && !g.pending && g.human == BLACK,
          "forced opening follows normal physical confirmation");
    check(g.forceMove() && g.human == WHITE && g.needsEngine(),
          "Move can switch sides again during a game");
    g.thinking = true;
    unsigned stale = g.epoch;
    g.board(start);
    check(g.epoch == stale + 1 && !g.thinking && !g.pending &&
              !g.ended && g.moves.empty() && g.human == WHITE &&
              !g.setup && g.history.size() == 1,
          "return to complete starting board resets game and sides during search");
    check(!g.engineResult("e7e5", stale),
          "automatic restart rejects results from the previous game");
    unsigned resetEpoch = g.epoch;
    g.board(start);
    check(g.epoch == resetEpoch,
          "duplicate starting boards do not repeatedly restart the game");
    partial = start;
    partial[SQ_E2] = '.';
    g.board(partial);
    check(g.epoch == resetEpoch && !g.forceMove(),
          "incomplete setup does not restart and cannot force a move");
    g.board(start);
    check(g.epoch == resetEpoch + 1 && g.moves.empty(),
          "restoring the last starting piece starts a fresh game");
    g.board(moveBoard(g.pos, "e2e4"));
    g.thinking = true;
    g.engineResult("e7e5", g.epoch);
    resetEpoch = g.epoch;
    g.board(reversed);
    check(g.epoch == resetEpoch + 1 && g.rotated && !g.pending &&
              g.physical == start && g.human == WHITE,
          "rotated starting board resets a pending engine reply");
    g.board(moveBoard(g.pos, "e2e4"));
    // Feed a raw rotated non-start arrangement before testing an ended game.
    Board rotatedMove = moveBoard(g.pos, "e2e4");
    std::reverse(rotatedMove.begin(), rotatedMove.end());
    g.board(rotatedMove);
    g.ended = true;
    resetEpoch = g.epoch;
    g.board(reversed);
    check(g.epoch == resetEpoch + 1 && !g.ended && !g.setup,
          "starting arrangement also restarts an ended game");
    g.start(WHITE);
    check(!g.forceMove(), "Move waits for a confirmed complete board");
    check(QueenMood::stage(250) == 0 && QueenMood::stage(249) == 1 &&
              QueenMood::stage(120) == 1 && QueenMood::stage(119) == 2,
          "queen positive boundaries match BeatVirgin");
    check(QueenMood::stage(0) == 2 && QueenMood::stage(-119) == 2 &&
              QueenMood::stage(-120) == 3 && QueenMood::stage(-249) == 3 &&
              QueenMood::stage(-250) == 4 && QueenMood::stage(-399) == 4 &&
              QueenMood::stage(-400) == 5,
          "queen negative boundaries match BeatVirgin");
    QueenMood mood;
    mood.beginSearch();
    check(mood.readAdaptive("info string adaptive halten player -50 after 260 accuracy 90 selected e2e4"),
          "queen reads adaptive evaluation after selected move");
    mood.finishSearch(500);
    check(mood.playerEval == 260 && QueenMood::stage(mood.playerEval) == 0,
          "adaptive after evaluation overrides strongest search line");
    mood.beginSearch();
    mood.finishSearch(150);
    check(mood.playerEval == -150 && !mood.haveAfter,
          "missing adaptive line falls back to negated engine score");
    check(!mood.readAdaptive("info string adaptive incomplete: using legal full-strength fallback") &&
              !mood.readAdaptive("info string adaptive hold after invalid"),
          "incomplete adaptive diagnostics cannot supply a queen evaluation");
    mood.finishSearch(SCORE_MATE - 5);
    check(mood.playerEval == -99997 && QueenMood::stage(mood.playerEval) == 5,
          "engine mating score maps to losing player queen");
    mood.finishSearch(-SCORE_MATE + 4);
    check(mood.playerEval == 99998 && QueenMood::stage(mood.playerEval) == 0,
          "engine losing mate maps to winning player queen");
    mood.flip();
    check(mood.playerEval == -99998 && !mood.haveAfter,
          "side switch reverses queen perspective");
    set(g, "7k/6Q1/5K2/8/8/8/8/8 b - - 0 1");
    g.finishCheck();
    check(g.ended && g.mood.playerEval == 100000,
          "human mating win sets queen to best state");
    g.human = BLACK;
    g.finishCheck();
    check(g.mood.playerEval == -100000,
          "human mating loss sets queen to worst state");
    set(g, "7k/5Q2/5K2/8/8/8/8/8 b - - 0 1");
    g.mood.playerEval = 600;
    g.finishCheck();
    check(g.ended && g.mood.playerEval == 0,
          "stalemate resets queen to neutral");
    g.start(WHITE);
    check(g.mood.playerEval == 0 && !g.mood.haveAfter,
          "new game clears previous queen evaluation");
    Board decoded{};
    uint8_t nut[34] = {1, 0x24};
    const char *codes = ".qkbpnRPrBNQK";
    for (int i = 0; i < 64; i++) {
      int code = int(std::strchr(codes, start[63 - i]) - codes);
      nut[2 + i / 2] |= uint8_t(code << (4 * (i % 2)));
    }
    check(chessnutFrame(nut, sizeof(nut), decoded) && decoded == start,
          "Chessnut nibbles decode H8-to-A1 into canonical coordinates");
    nut[4] = 255;
    check(!chessnutFrame(nut, sizeof(nut), decoded),
          "invalid Chessnut piece codes are rejected");
    std::string mode = "s";
    for (int i = 0; i < 64; i++)
      mode += start[(i / 8) * 8 + 7 - i % 8];
    auto positionWire = modePosition(start);
    check(modeFrame(positionWire.data(),positionWire.size(),decoded) && decoded==start,
          "full starting position round-trips without a reset command");
    auto wire = modeCommand(mode);
    check(modeFrame(wire.data(), wire.size(), decoded) && decoded == start,
          "ChessLink parity/checksum and reversed files decode correctly");
    wire[8] ^= 1;
    check(!modeFrame(wire.data(), wire.size(), decoded),
          "corrupt ChessLink checksum cannot replace the board");
    uint64_t mask = (1ULL << SQ_E2) | (1ULL << SQ_E4);
    auto led = chessnutLeds(mask);
    check(led.size() == 10 && led[8] == 8 && led[6] == 8,
          "Chessnut LEDs use the confirmed rank/file bit convention");
    auto ml = modeLeds(mask);
    check(ml.size() == 167 && (ml[0] & 127) == 'L' && (ml[1] & 127) == '0' &&
              (ml[2] & 127) == '1',
          "ChessLink uses the full 167-byte LED grid command");
    std::cout << "RESULT " << passed << " checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
