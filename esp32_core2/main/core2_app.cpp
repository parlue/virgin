
#include "bitboard.h"
#include "board_link.h"
#include "driver/uart.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "esp_pthread.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "game_model.h"
#include "piece_tiles.h"
#include "queen_tiles.h"
#include "search.h"
#include "tt.h"
#include "uci.h"
#include "weights.h"
#include <M5Unified.h>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <sstream>
#include <streambuf>

namespace {
using namespace core2;
struct Command {
  std::string text;
  unsigned epoch;
};
struct Reply {
  std::string text;
  unsigned epoch;
};
std::mutex ioLock;
std::condition_variable inputReady;
std::deque<Command> commands;
std::deque<Reply> replies;
std::atomic<unsigned> searchEpoch{0}, controlEpoch{0}, commandEpoch{0};
bool adaptiveMode = true, engineReady = false;
int botElo = 0; // zero means Adaptive or full strength, not an Elo-limited bot
bool engineOnline = false;
bool virtualBoard = false;
Game game;
struct Thoughts {
  int depth = 0, cp = 0;
  std::string pv;
  bool white = true;
} thoughts;
bool searchWhite = true;
struct Input : std::streambuf {
  std::string line;
  int_type underflow() override {
    if (gptr() && gptr() < egptr())
      return traits_type::to_int_type(*gptr());
    std::unique_lock<std::mutex> lock(ioLock);
    inputReady.wait(lock, [] { return !commands.empty(); });
    auto item = std::move(commands.front());
    commands.pop_front();
    commandEpoch = item.epoch;
    if (item.text == "isready") controlEpoch = item.epoch;
    if (item.text.rfind("go ", 0) == 0)
      searchEpoch = item.epoch;
    line = std::move(item.text);
    line += '\n';
    setg(line.data(), line.data(), line.data() + line.size());
    return traits_type::to_int_type(*gptr());
  }
};
struct Output : std::streambuf {
  std::string line;
  int_type overflow(int_type ch) override {
    if (traits_type::eq_int_type(ch, traits_type::eof()))
      return traits_type::not_eof(ch);
    char c = traits_type::to_char_type(ch);
    if (c == '\n') {
      // The UI consumes only complete lines, never from a BLE callback.
      std::lock_guard<std::mutex> lock(ioLock);
      if (line == "readyok") replies.push_back({line, controlEpoch.load()});
      if (line.rfind("info string style ",0)==0 ||
          line.rfind("info string elo ",0)==0 ||
          line.rfind("info string strength limited ",0)==0 ||
          line.rfind("info string full strength",0)==0 ||
          line.rfind("info string Adaptive ",0)==0)
        replies.push_back({line,commandEpoch.load()});
      if (line.rfind("info depth ", 0) == 0 &&
          (line.find(" multipv ") == std::string::npos ||
           line.find(" multipv 1 ") != std::string::npos)) {
        for (auto it = replies.begin(); it != replies.end();) {
          if (it->text.rfind("info depth ", 0) == 0)
            it = replies.erase(it);
          else
            ++it;
        }
        replies.push_back({line, searchEpoch.load()});
      }
      if (line.rfind("info string adaptive ", 0) == 0 ||
          line.rfind("bestmove ", 0) == 0 ||
          line.rfind("info string firmware", 0) == 0)
        replies.push_back({line, searchEpoch.load()});
      line.clear();
    } else
      line += c;
    return ch;
  }
  int sync() override { return 0; }
};
void command(const std::string &text, unsigned epoch) {
  std::lock_guard<std::mutex> lock(ioLock);
  commands.push_back({text, epoch});
  inputReady.notify_one();
}
void engineTask(void *) {
  Input buffer;
  Output sink;
  std::istream input(&buffer);
  std::ostream output(&sink);
  uciRun(input, output);
  vTaskDelete(nullptr);
}
void searchYield() { vTaskDelay(1); }
bool initEngine() {
  if (!esp_psram_is_initialized() || esp_psram_get_size() < 2 * 1024 * 1024)
    return false;
  esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
  cfg.prio = 4;
  cfg.pin_to_core = 1;
  cfg.inherit_cfg = true;
  cfg.thread_name = "virgin_search";
  cfg.stack_size = 72 * 1024;
  cfg.stack_alloc_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  if (esp_pthread_set_cfg(&cfg) != ESP_OK)
    return false;
  init_bitboards();
  init_zobrist();
  init_weights();
  initSearch();
  TT.resizeKiB(1024);
  setSearchPoll(searchYield);
  uciInit(0, nullptr); // allocates the permanent internal search stack
  return xTaskCreatePinnedToCore(engineTask, "virgin_uci", 12288, nullptr, 3,
                                 nullptr, 1) == pdPASS;
}
enum class Menu { None, Boards, Bots };
constexpr int botMin = 900, botMax = 2500, botStep = 100;
Menu menu = Menu::None;
M5Canvas frame(&M5.Display);
bool dirty = true;
int batteryLevel = -1;
bool batteryCharging = false;
unsigned seenRevision = ~0U;
std::string previousStatus;
bool previousConnected = false;
constexpr uint16_t bg = 0x0884, panel = 0x1128, ink = 0xE79F, muted = 0x8CB4,
                   accent = 0x5E0F, button = 0x4BD5;
uint32_t millisNow() { return uint32_t(esp_timer_get_time() / 1000); }
void text(int x, int y, const std::string &s, uint16_t color = ink,
          bool small = false) {
  if (small)
    frame.setFont(&fonts::Font0);
  else
    frame.setFont(&fonts::Font2);
  frame.setTextColor(color);
  frame.setTextSize(1);
  frame.drawString(s.c_str(), x, y);
}
void wrapped(int x, int y, std::string s, int width, uint16_t color = ink,
             int rows = 3, bool small = false) {
  if (small)
    frame.setFont(&fonts::Font0);
  else
    frame.setFont(&fonts::Font2);
  std::string line;
  std::istringstream words(s);
  std::string word;
  int row = 0;
  while (words >> word) {
    std::string next = line.empty() ? word : line + " " + word;
    if (frame.textWidth(next.c_str()) > width && !line.empty()) {
      text(x, y + row * 17, line, color, small);
      line = word;
      if (++row >= rows)
        return;
    } else
      line = next;
  }
  if (row < rows)
    text(x, y + row * 17, line, color, small);
}
void pill(int x, int y, int w, int h, const std::string &label,
          uint16_t color = button) {
  frame.fillRoundRect(x, y, w, h, 7, color);
  frame.setFont(&fonts::Font2);
  frame.setTextColor(ink);
  int tw = frame.textWidth(label.c_str());
  frame.drawString(label.c_str(), x + (w - tw) / 2, y + (h - 16) / 2);
}
void configureEngine(bool newGame) {
  engineReady = false;
  // Discard queued searches from the previous UI epoch before reconfiguring.
  { std::lock_guard<std::mutex> lock(ioLock); commands.clear(); }
  command("stop", game.epoch);
  if (newGame) command("ucinewgame", game.epoch);
  // Leave Adaptive before restoring a concrete style/handicap. Clearing the
  // old handicap first makes Bot -> Full -> Adaptive transitions independent.
  command("setoption name Adaptive value false",game.epoch);
  command("setoption name UCI_LimitStrength value false",game.epoch);
  command(std::string("setoption name Style value ")+(botElo ? "Fire" : "Normal"),game.epoch);
  if (botElo) {
    command("setoption name UCI_Elo value "+std::to_string(botElo),game.epoch);
    command("setoption name UCI_LimitStrength value true",game.epoch);
  } else if (adaptiveMode) command("setoption name Adaptive value true",game.epoch);
  // Only start the next search after the old worker has joined and options applied.
  command("isready", game.epoch);
}
void resetEngineGame() {
  thoughts = {};
  configureEngine(true);
  dirty = true;
  printf("CORE2 NEW_GAME epoch=%u adaptive=%d\n",game.epoch,adaptiveMode);
}
void newGameButton() {
  const auto link = linkView();
  seenRevision = link.revision;
  game.start(Color::WHITE);
  resetEngineGame();
  menu = Menu::None;
  const bool sent = linkStartingPosition();
  if (!sent && link.haveBoard) game.board(link.board);
  if (virtualBoard) game.board(Game::diagram(game.pos));
  printf("CORE2 NEW_GAME_BUTTON position=%d connection_preserved=%d\n",sent,link.connected);
}
void modeButton() {
  adaptiveMode = !adaptiveMode;
  botElo = 0;
  ++game.epoch;
  game.thinking = false;
  game.mood.beginSearch();
  configureEngine(false);
  dirty = true;
  printf("CORE2 MODE adaptive=%d epoch=%u\n",adaptiveMode,game.epoch);
}
void botButton() {
  menu = menu == Menu::Bots ? Menu::None : Menu::Bots;
  dirty = true;
}
void chooseBot(int elo) {
  if (elo<botMin || elo>botMax || (elo-botMin)%botStep) return;
  botElo = elo; adaptiveMode = false;
  ++game.epoch; game.thinking = false; game.mood.beginSearch();
  configureEngine(false);
  menu = Menu::None; dirty = true;
  printf("CORE2 BOT elo=%d style=Fire stage=4 epoch=%u\n",botElo,game.epoch);
}
void receiveBoard(const Board &board) {
  unsigned previousEpoch = game.epoch;
  game.board(board);
  if (game.epoch != previousEpoch)
    resetEngineGame();
  dirty = true;
}
void moveButton() {
  const auto link = linkView();
  if (!engineOnline || !engineReady || link.calibrating || (!link.connected && !virtualBoard))
    return;
  if (game.thinking)
    command("stop", game.epoch);
  else if (game.forceMove()) {
    thoughts = {};
    printf("CORE2 SWITCH human=%s epoch=%u\n",
           game.human == Color::WHITE ? "white" : "black", game.epoch);
  }
  dirty = true;
}
void updateBattery() {
  static bool first = true;
  static uint32_t lastRead = 0;
  const uint32_t now = millisNow();
  if (!first && now-lastRead < 5000) return;
  first = false;
  lastRead = now;
  const int raw = M5.Power.getBatteryLevel();
  const int level = raw < 0 ? -1 : std::min(raw,100);
  const bool charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
  if (level != batteryLevel || charging != batteryCharging) {
    batteryLevel = level;
    batteryCharging = charging;
    dirty = true;
  }
}
void drawBattery() {
  const uint16_t color = batteryLevel >= 0 && batteryLevel <= 20 ? 0xFB64 : accent;
  char label[16];
  if (batteryLevel < 0) snprintf(label,sizeof(label),"--%%");
  else snprintf(label,sizeof(label),"%d%%",batteryLevel);
  text(282-int(std::strlen(label))*6,8,label,ink,true);
  frame.drawRoundRect(286,6,26,13,2,ink);
  frame.fillRect(312,10,3,5,ink);
  if (batteryLevel >= 0) {
    const int fill = (20*batteryLevel+99)/100;
    if (fill) frame.fillRect(289,9,fill,7,color);
  } else text(296,9,"?",muted,true);
  if (batteryCharging) {
    frame.drawLine(301,8,296,13,ink);
    frame.drawLine(296,13,301,13,ink);
    frame.drawLine(301,13,296,17,ink);
  }
}
void draw(const LinkView &link) {
  frame.startWrite();
  frame.fillScreen(bg);
  text(8, 3, "Virgin");
  frame.fillRoundRect(184, 4, 70, 17, 6, panel);
  text(69, 8, "A:Neu B:Mod C:Bot", muted, true);
  text(botElo ? 194 : (adaptiveMode ? 198 : 189),8,
       botElo ? std::to_string(botElo)+" Elo" : (adaptiveMode ? "ADAPTIV" : "VOLLE KRAFT"),accent,true);
  drawBattery();
  frame.drawFastHLine(8, 23, 304, panel);
  Board board = game.displayBoard();
  uint64_t diff = game.differences();
  Move highlighted = game.pending ? game.pending : game.last;
  frame.fillRoundRect(5, 25, 206, 206, 4, panel);
  for (int display = 0; display < 64; display++) {
    int rank = 7 - display / 8, file = display % 8;
    int square =
        game.human == Color::BLACK ? 63 - (rank * 8 + file) : rank * 8 + file;
    int x = 8 + file * 25, y = 28 + (display / 8) * 25;
    bool dark = (file + display / 8) & 1;
    int piece = -1;
    const char *order = "KQRBNPkqrbnp";
    const char *where = std::strchr(order, board[square]);
    if (where && board[square] != '.')
      piece = int(where - order);
    if (piece >= 0)
      frame.pushImage(x, y, 25, 25, PieceTiles[piece][dark ? 1 : 0]);
    else
      frame.fillRect(x, y, 25, 25, dark ? 0x7D19 : 0xEF9F);
    if ((diff & (1ULL << square)) && !game.setup)
      frame.drawRect(x, y, 25, 25, 0xFB64);
    if (highlighted &&
        (square == from_sq(highlighted) || square == to_sq(highlighted))) {
      frame.drawRect(x, y, 25, 25, accent);
      frame.drawRect(x + 1, y + 1, 23, 23, accent);
    }
  }
  for (int i = 0; i < 8; i++) {
    text(17 + i * 25, 232,
         std::string(1, char((game.human == Color::BLACK ? 'h' : 'a') +
                             (game.human == Color::BLACK ? -i : i))),
         muted, true);
  }
  frame.fillCircle(227, 36, 3, link.connected ? accent : 0xFB64);
  text(236, 31, link.connected ? "Verbunden" : "Kein Brett", muted, true);
  std::string status = !engineOnline     ? "Engine konnte nicht starten"
                       : (!link.connected || !link.haveBoard) ? link.status
                                         : game.message;
  if (thoughts.depth && !game.setup && !game.ended) {
    text(221, 52,
         game.thinking ? "Rechnet..."
                       : (game.pending ? "Nachziehen" : "Du am Zug"),
         accent);
    text(221, 80, "Tiefe " + std::to_string(thoughts.depth), ink, true);
    int score = thoughts.white ? thoughts.cp : -thoughts.cp;
    char eval[32];
    if (std::abs(score) >= 30000)
      snprintf(eval, sizeof(eval), "%sMatt", score >= 0 ? "+" : "-");
    else
      snprintf(eval, sizeof(eval), "%+.2f", score / 100.0);
    text(221, 99, eval, ink, true);
    text(221, 118, "Weiss", muted, true);
    text(221, 145, thoughts.pv.substr(0, 14), accent, true);
    text(221, 157,
         thoughts.pv.substr(std::min<size_t>(14, thoughts.pv.size()), 14),
         accent, true);
  } else {
    wrapped(221, 54, status, 92, game.ended ? accent : ink, 2, true);
    text(221, 101, "Du:", muted, true);
    text(221, 118, game.human == Color::WHITE ? "Weiss" : "Schwarz", ink, true);
    if (highlighted)
      text(221, 145, move_to_uci(highlighted), accent, true);
  }
  frame.pushImage(270, 80, QueenWidth, QueenHeight,
                  QueenTiles[QueenMood::stage(game.mood.playerEval)]);
  pill(220, 171, 92, 27, "Move");
  pill(220, 205, 92, 28, link.connected ? "Brett" : "Verbinden", panel);
  if (menu == Menu::Boards) {
    frame.fillRect(0, 24, 320, 216, bg);
    text(10, 30, "Brett verbinden");
    if (link.scanning || link.connecting) {
      wrapped(12, 71, link.status, 290);
    } else if (!link.count) {
      wrapped(12, 72,
              "Chessnut, ChessLink oder Certabo einschalten und in Reichweite bringen.",
              290);
    } else
      for (unsigned i = 0; i < link.count; i++) {
        int y = 54 + int(i) * 29;
        frame.fillRoundRect(8, y, 304, 27, 5, panel);
        text(16, y + 2, link.peers[i].name);
        std::string suffix = link.peers[i].address.substr(12);
        text(265, y + 11, suffix, muted, true);
      }
    pill(8, 207, 151, 28, "Neu suchen");
    pill(174, 207, 138, 28, "Zurueck", panel);
  }
  if (menu == Menu::Bots) {
    frame.fillRect(0,24,320,216,bg);
    text(10,30,"Elo auswaehlen");
    text(222,35,"Fire / Stufe 4",accent,true);
    for (int index=0;index<17;++index) {
      const int elo=botMin+index*botStep;
      const int x=8+(index%4)*78, y=49+(index/4)*38;
      pill(x,y,70,32,std::to_string(elo),elo==botElo ? button : panel);
      if (elo==botElo) frame.drawRoundRect(x,y,70,32,7,accent);
    }
    pill(86,201,226,32,"Zurueck",panel);
  }
  frame.endWrite();
  // A completed frame reaches the LCD in one transfer, without exposing clears.
  static bool first = true;
  static int previousBattery = -2;
  static bool previousCharging = false, previousAdaptive = true;
  static int previousBotElo = 0;
  static Board previousBoard{};
  static uint64_t previousDiff = 0;
  static Move previousHighlight = 0;
  static Color previousHuman = Color::WHITE;
  static Menu previousMenu = Menu::None;
  bool panelOnly = !first && menu == Menu::None && previousMenu == Menu::None &&
                   board == previousBoard && diff == previousDiff &&
                   highlighted == previousHighlight &&
                   game.human == previousHuman;
  M5.Display.startWrite();
  if (panelOnly) {
    if (previousBattery != batteryLevel || previousCharging != batteryCharging || previousAdaptive != adaptiveMode || previousBotElo != botElo) {
      M5.Display.setClipRect(180,0,140,24);
      frame.pushSprite(0,0);
    }
    M5.Display.setClipRect(216, 24, 104, 216);
  }
  frame.pushSprite(0, 0);
  M5.Display.clearClipRect();
  M5.Display.endWrite();
  first = false;
  previousAdaptive = adaptiveMode;
  previousBotElo = botElo;
  previousBattery = batteryLevel;
  previousCharging = batteryCharging;
  previousBoard = board;
  previousDiff = diff;
  previousHighlight = highlighted;
  previousHuman = game.human;
  previousMenu = menu;
  dirty = false;
}
void tap(int x, int y) {
  if (y >= 240) return;
  if (menu == Menu::None && y < 24 && x >= 70 && x < 255) {
    if (x < 110) newGameButton();
    else if (x >= 146 && x < 184) botButton();
    else modeButton();
    return;
  }
  if (menu == Menu::Bots) {
    if (x>=86 && x<312 && y>=201 && y<233) menu = Menu::None;
    else if (x>=8 && x<312 && y>=49 && y<233) {
      const int column=(x-8)/78, row=(y-49)/38;
      const int index=row*4+column;
      if ((x-8)%78<70 && (y-49)%38<32 && index<17)
        chooseBot(botMin+index*botStep);
    }
  } else if (menu == Menu::Boards) {
    auto l = linkView();
    if (y >= 54 && y < 199 && x >= 8 && x < 312 && !l.scanning &&
        !l.connecting) {
      unsigned index = unsigned((y - 54) / 29);
      if (index < l.count)
        linkChoose(index);
    } else if (y >= 207 && x < 160)
      linkScan();
    else if (y >= 207)
      menu = Menu::None;
  } else {
    if (x >= 220 && y >= 171 && y < 198)
      moveButton();
    else if (x >= 220 && y >= 205) {
      menu = Menu::Boards;
      linkScan();
    }
  }
  dirty = true;
}
void screenshot() {
  printf("VIRGIN_SCREEN 320 240 RGB565\n");
  fflush(stdout);
  uint16_t row[320];
  for (int y = 0; y < 240; y++) {
    M5.Display.readRect(0, y, 320, 1, row);
    uart_write_bytes(UART_NUM_0, row, sizeof(row));
  }
  uart_wait_tx_done(UART_NUM_0, pdMS_TO_TICKS(20000));
  printf("\nVIRGIN_SCREEN_END\n");
}
void diagnostics() {
  static std::string line;
  uint8_t bytes[256];
  int n = uart_read_bytes(UART_NUM_0, bytes, sizeof(bytes), 0);
  for (int i = 0; i < n; i++) {
    char c = bytes[i];
    if (c == '\r')
      continue;
    if (c != '\n') {
      if (line.size() < 256)
        line += c;
      continue;
    }
    if (line == "status") {
      auto l = linkView();
      std::string state(game.physical.begin(), game.physical.end());
      printf("CORE2 STATUS engine=%d depth=%d connected=%d scanning=%d "
             "connecting=%d certabo=%d spectrum=%d calibrating=%d setup=%d thinking=%d ended=%d epoch=%u "
             "human=%s pending=%s moves=%u adaptive=%s bot_elo=%d bot_stage=%d menu=%d mood=%d player_eval=%d board=%s "
             "free_internal=%u free_psram=%u battery=%d charging=%d\n",
             engineOnline, thoughts.depth, l.connected, l.scanning,
             l.connecting, l.certabo, l.spectrum, l.calibrating,
             game.setup, game.thinking, game.ended, game.epoch,
             game.human == Color::WHITE ? "white" : "black",
             game.pending ? move_to_uci(game.pending).c_str() : "none",
             unsigned(game.moves.size()), adaptiveMode ? "true" : "false", botElo, botElo ? 4 : 0, int(menu), QueenMood::stage(game.mood.playerEval),
             game.mood.playerEval, state.c_str(),
             unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
             batteryLevel, batteryCharging);
    } else if (line == "screenshot")
      screenshot();
#ifdef VIRGIN_CORE2_DIAGNOSTICS
    else if (line == "newgame") newGameButton();
    else if (line == "mode") modeButton();
    else if (line == "bots") botButton();
    else if (line.rfind("tap ", 0) == 0) {
      int x, y;
      if (sscanf(line.c_str(), "tap %d %d", &x, &y) == 2)
        tap(x, y);
    } else if (line.rfind("testboard ", 0) == 0 && line.size() == 74) {
      Board b;
      std::copy(line.begin() + 10, line.end(), b.begin());
      virtualBoard = true;
      receiveBoard(b);
      dirty = true;
    }
#endif
    line.clear();
  }
}
} // namespace
extern "C" void app_main() {
  uart_driver_install(UART_NUM_0, 4096, 4096, 0, nullptr, 0);
  heap_caps_malloc_extmem_enable(2048);
  auto cfg = M5.config();

  cfg.internal_imu = false;
  cfg.internal_rtc = false;
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  // Reserve the contiguous search stack before display/BLE allocations.
  try {
    engineOnline = initEngine();
  } catch (const std::exception &e) {
    printf("CORE2 ENGINE_INIT_ERROR %s largest=%u\n", e.what(),
           unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
  }
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Display.setBrightness(150);
  M5.Display.setSwapBytes(true);
  frame.setPsram(true);
  frame.setColorDepth(16);
  frame.setSwapBytes(true);
  if (!frame.createSprite(320, 240)) {
    M5.Display.drawString("Anzeigespeicher fehlt", 10, 40);
    return;
  }
  text(10, 40, "Virgin startet...");
  game.start(Color::WHITE);
  if (engineOnline) {
    configureEngine(true);
  }
  printf("CORE2 READY engine=%d psram=%u free_internal=%u adaptive=true\n",
         engineOnline, unsigned(esp_psram_get_size()),
         unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
  linkStart();
  uint32_t lastDraw = 0;
  while (true) {
    M5.update();
    updateBattery();
    if (M5.BtnA.wasClicked()) newGameButton();
    if (M5.BtnB.wasClicked()) modeButton();
    if (M5.BtnC.wasClicked()) botButton();
    auto touch = M5.Touch.getDetail();
    if (touch.wasClicked()) tap(touch.x,touch.y);
    auto link = linkView();
    if (virtualBoard) {
      link.connected = true;
      link.haveBoard = false;
      link.status = "Testbrett";
    }
    if (link.connected != previousConnected || link.status != previousStatus) {
      dirty = true;
      previousConnected = link.connected;
      previousStatus = link.status;
      if (link.connected && menu == Menu::Boards)
        menu = Menu::None;
    }
    if (link.connected && link.haveBoard && seenRevision != link.revision) {
      seenRevision = link.revision;
      receiveBoard(link.board);
      dirty = true;
    }
    std::deque<Reply> received;
    {
      std::lock_guard<std::mutex> lock(ioLock);
      received.swap(replies);
    }
    for (const auto &reply : received) {
      if (reply.epoch != game.epoch)
        continue;
      if (reply.text == "readyok") { engineReady = true; continue; }
      if (reply.text.rfind("info string style ",0)==0 ||
          reply.text.rfind("info string elo ",0)==0 ||
          reply.text.rfind("info string strength limited ",0)==0 ||
          reply.text.rfind("info string full strength",0)==0 ||
          reply.text.rfind("info string Adaptive ",0)==0) {
        printf("CORE2 ENGINE_CONFIG %s\n",reply.text.c_str());
        continue;
      }
      if (reply.text.rfind("info depth ", 0) == 0) {
        std::istringstream info(reply.text);
        std::string token;
        int multipv = 1;
        Thoughts next = thoughts;
        next.white = searchWhite;
        while (info >> token) {
          if (token == "depth")
            info >> next.depth;
          else if (token == "multipv")
            info >> multipv;
          else if (token == "cp")
            info >> next.cp;
          else if (token == "mate") {
            int distance = 0;
            info >> distance;
            next.cp = distance >= 0 ? SCORE_MATE - 2 * distance
                                    : -SCORE_MATE - 2 * distance;
          }
          else if (token == "pv") {
            std::getline(info, next.pv);
            if (!next.pv.empty())
              next.pv.erase(0, 1);
            break;
          }
        }
        if (multipv == 1) {
          thoughts = next;
          dirty = true;
        }
      }
      game.mood.readAdaptive(reply.text);
      if (reply.text.rfind("bestmove ", 0) == 0) {
        std::istringstream in(reply.text);
        std::string word, move;
        in >> word >> move;
        if (game.engineResult(move, reply.epoch)) {
          game.mood.finishSearch(thoughts.cp);
          dirty = true;
          printf("CORE2 ENGINE_MOVE %s epoch=%u\n", move.c_str(), reply.epoch);
        }
      }
    }
    if (engineOnline && engineReady && link.connected && (link.haveBoard || virtualBoard) &&
        game.needsEngine()) {
      searchWhite = game.pos.sideToMove() == Color::WHITE;
      game.thinking = true;
      game.mood.beginSearch();
      game.message = "Virgin denkt...";
      command(game.positionCommand(), game.epoch);
      command("go movetime 2500", game.epoch);
      dirty = true;
    }
    linkLeds(link.connected ? game.ledSquares() : 0, game.mood.playerEval);
    diagnostics();
    if (dirty && millisNow() - lastDraw > 80) {
      draw(link);
      lastDraw = millisNow();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
