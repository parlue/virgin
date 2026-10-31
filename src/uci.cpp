// Virgin - UCI chess engine
// UCI front end: options, position handling, go/perft/bench commands.
#include "uci.h"
#include "adaptive.h"
#include "adaptive_params.h"

#include "botdata.h"
#include "eval.h"
#include "movegen.h"
#include "position.h"
#include "search.h"
#include "tt.h"
#include "weights.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <random>
#include <array>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr const char* ENGINE_NAME   = "Virgin 0.7.0";
constexpr const char* ENGINE_AUTHOR = "Dirk Sommerfeld / built with Cline";

constexpr const char* START_FEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

Position    g_pos;
bool        g_debug = false;
std::string g_exeDir = ".";
std::string g_weightsFile;
std::string g_currentBot;
std::vector<Key> g_gameKeys;  // Zobrist keys of the moves played so far

struct Option {
    std::string name;
    std::string type;  // spin | check | string | button | combo
    std::string def;
    std::string value;
    int         min = 0;
    int         max = 0;
    int         weightIndex = -1;
    std::vector<std::string> vars;
};

std::vector<Option> g_options;
bool g_adaptive = false;
std::vector<Option> g_savedOptions;
std::vector<int> g_savedWeights;
std::array<int, GROUP_NB> g_savedKnowledge{};
Strength g_savedStrength;
int g_savedThreads = 0;
size_t g_savedHash = 32;
AdaptiveState g_adaptiveState, g_adaptiveBefore;
std::string g_adaptiveLastFen;
int g_adaptiveLastPly = -1;
Color g_adaptiveLastColor = WHITE;
Key g_adaptiveChosenKey = 0;
double g_adaptiveRoll = 1.0, g_adaptivePick = 0.0;
std::mt19937 g_adaptiveRng(uint32_t(std::chrono::steady_clock::now().time_since_epoch().count()));

void resetAdaptiveGame() {
    g_adaptiveState = {}; g_adaptiveBefore = {};
    g_adaptiveLastFen.clear(); g_adaptiveLastPly = -1; g_adaptiveChosenKey = 0;
}

struct BotInfo {
    std::string id;
    std::string style;
    std::string display;
    std::string tags;
    int         elo = 0;
};

std::vector<BotInfo> g_bots;

std::string trimmed(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos)
        return "";
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string dataFile(const std::string& name) {
    const std::string candidates[] = {
        g_exeDir + "/data/" + name,
        g_exeDir + "\\data\\" + name,
        "data/" + name,
        name,
    };
    for (const std::string& c : candidates) {
        std::ifstream probe(c);
        if (probe.good())
            return c;
    }
    return "";
}

void addOption(const Option& o) { g_options.push_back(o); }

void registerOptions() {
    g_options.clear();

    addOption({"Hash", "spin", "32", "32", 1, 4096, -1, {}});
    // 0 = automatic: as many threads as there are cores, capped at 4 (the
    // measured sweet spot). The default shown is the resolved value.
    addOption({"Threads", "spin", std::to_string(threadCount()).c_str(),
               std::to_string(threadCount()).c_str(), 0, 64, -1, {}});
    addOption({"Adaptive", "check", "false", "false", 0, 0, -1, {}});
    addOption({"Ponder", "check", "false", "false", 0, 0, -1, {}});
    addOption({"UCI_LimitStrength", "check", "false", "false", 0, 0, -1, {}});
    addOption({"UCI_Elo", "spin", "2800", "2800", 700, 3000, -1, {}});
    addOption({"Style", "combo", "Ausgeglichen", "Ausgeglichen", 0, 0, -1,
               {"Furchtsam", "Vorsichtig", "Ausgeglichen", "Angriffslustig", "Stier"}});
    addOption({"Boldness", "spin", "30", "30", 10, 50, -1, {}});
    addOption({"Bot", "string", "<empty>", "<empty>", 0, 0, -1, {}});
    addOption({"WeightsFile", "string", "<empty>", "<empty>", 0, 0, -1, {}});
    addOption({"Clear Hash", "button", "", "", 0, 0, -1, {}});
    addOption({"ReloadWeights", "button", "", "", 0, 0, -1, {}});
    addOption({"ResetWeights", "button", "", "", 0, 0, -1, {}});
    addOption({"ShowWeights", "button", "", "", 0, 0, -1, {}});
    addOption({"ListBots", "button", "", "", 0, 0, -1, {}});

    // Every registered evaluation / style weight becomes a spin option, so any
    // GUI (or the hosting app) can drive the playing style from the outside.
    for (int i = 0; i < weight_count(); ++i) {
        const WeightDef& d = weight_def(i);
        Option o;
        o.name        = d.name;
        o.type        = "spin";
        o.def         = std::to_string(d.def);
        o.value       = std::to_string(d.def);
        o.min         = d.min;
        o.max         = d.max;
        o.weightIndex = i;
        g_options.push_back(std::move(o));
    }
}

Option* findOption(const std::string& name) {
    for (Option& o : g_options)
        if (o.name == name)
            return &o;
    return nullptr;
}

void printUci() {
    std::cout << "id name " << ENGINE_NAME << "\n";
    std::cout << "id author " << ENGINE_AUTHOR << "\n";
    for (const Option& o : g_options) {
        if (o.type == "button") {
            std::cout << "option name " << o.name << " type button\n";
            continue;
        }
        std::cout << "option name " << o.name << " type " << o.type << " default " << o.def;
        if (o.type == "spin")
            std::cout << " min " << o.min << " max " << o.max;
        if (o.type == "combo")
            for (const std::string& v : o.vars)
                std::cout << " var " << v;
        std::cout << "\n";
    }
    std::cout << "uciok" << std::endl;
}

void syncWeightOptions() {
    for (Option& o : g_options)
        if (o.weightIndex >= 0)
            o.value = std::to_string(weight_value(weight_def(o.weightIndex).name));
}

void setAdaptive(bool enabled) {
    if (g_adaptive == enabled) return;
    if (enabled) {
        g_savedOptions = g_options;
        g_savedWeights.clear();
        for (int i = 0; i < weight_count(); ++i)
            g_savedWeights.push_back(weight_value(weight_def(i).name));
        std::copy_n(EW.knowledge, GROUP_NB, g_savedKnowledge.begin());
        g_savedStrength = g_strength;
        g_savedThreads = std::atoi(findOption("Threads")->value.c_str());
        g_savedHash = TT.sizeMb();
        applyStyle("Angriffslustig");
        clearStrength(); setThreads(0); TT.resize(TalParams::suche_hash_mb);
        g_adaptive = true;
        findOption("Adaptive")->value = "true";
        syncWeightOptions();
        std::cout << "info string Adaptive on: full strength, five candidates, fixed attack profile, hash 32 MB, automatic threads" << std::endl;
    } else {
        for (int i = 0; i < weight_count(); ++i) set_weight_index(i, g_savedWeights[size_t(i)]);
        std::copy(g_savedKnowledge.begin(), g_savedKnowledge.end(), EW.knowledge);
        g_strength = g_savedStrength;
        setThreads(g_savedThreads); TT.resize(g_savedHash);
        g_options = g_savedOptions;
        g_adaptive = false;
        std::cout << "info string Adaptive off: previous settings restored" << std::endl;
    }
    resetAdaptiveGame(); newSearchGame();
}

void applyAdaptiveChoice(SearchResult& result) {
    if (result.candidates.empty()) {
        std::cout << "info string adaptive incomplete: using legal full-strength fallback" << std::endl;
        return;
    }
    const std::string fen = g_pos.fen();
    const int ply = int(std::min<int64_t>(int64_t(g_pos.fullmoveNumber() - 1) * 2
                                        + int(g_pos.sideToMove() == BLACK), 1000000000));
    if (fen == g_adaptiveLastFen) {
        g_adaptiveState = g_adaptiveBefore; // re-analysis is not a new player move
    } else {
        if (g_adaptiveLastPly >= 0 && (ply != g_adaptiveLastPly + 2 || g_pos.sideToMove() != g_adaptiveLastColor
            || (g_gameKeys.size() >= 3 && g_gameKeys[g_gameKeys.size() - 2] != g_adaptiveChosenKey)))
            resetAdaptiveGame();
        g_adaptiveBefore = g_adaptiveState;
        std::uniform_real_distribution<double> random(0.0, 1.0);
        g_adaptiveRoll = random(g_adaptiveRng); g_adaptivePick = random(g_adaptiveRng);
        g_adaptiveLastFen = fen; g_adaptiveLastPly = ply; g_adaptiveLastColor = g_pos.sideToMove();
    }
    const auto choice = chooseAdaptive(g_pos, result.candidates, ply, g_adaptiveState,
                                       g_adaptiveRoll, g_adaptivePick);
    const auto& candidate = result.candidates[choice.index];
    const Move strongest = result.candidates.front().move;
    result.best = candidate.move; result.score = candidate.score; result.pv = candidate.pv;
    Position chosen = g_pos; chosen.applyMove(result.best); g_adaptiveChosenKey = chosen.key();
    std::cout << "info string adaptive " << choice.mode << " player " << choice.playerEval
              << " after " << choice.afterEval << " accuracy " << choice.accuracy
              << " best " << move_to_uci(strongest) << " selected " << move_to_uci(result.best)
              << " samples " << g_adaptiveState.accuracy.size() << " helped " << g_adaptiveState.helped << " adjusts " << g_adaptiveState.adjusts << std::endl;
    std::cout << "info depth " << result.depth << " score cp " << result.score
              << " nodes " << result.nodes << " time " << result.timeMs << " pv";
    for (Move move : result.pv) std::cout << ' ' << move_to_uci(move);
    std::cout << std::endl;
}

void loadBots() {
    g_bots.clear();
    const std::string path = dataFile("bots.txt");
    if (path.empty())
        return;

    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        const size_t hash = line.find('#');
        if (hash != std::string::npos)
            line = line.substr(0, hash);
        line = trimmed(line);
        if (line.empty())
            continue;

        std::istringstream ls(line);
        BotInfo b;
        if (!(ls >> b.id >> b.elo >> b.style))
            continue;
        std::string display;
        if (ls >> std::ws && ls.peek() == '"') {
            ls.get();
            std::getline(ls, display, '"');
            b.display = display;
        }
        ls >> b.tags;
        if (b.display.empty())
            b.display = b.id;
        g_bots.push_back(std::move(b));
    }
}

const BotInfo* findBot(const std::string& id) {
    for (const BotInfo& b : g_bots)
        if (b.id == id)
            return &b;
    return nullptr;
}

void printBots() {
    if (g_bots.empty()) {
        std::cout << "info string no bot catalog found (expected data/bots.txt next to the binary)"
                  << std::endl;
        return;
    }
    for (const BotInfo& b : g_bots)
        std::cout << "info string bot " << b.id << " " << b.elo << " " << b.style << " \""
                  << b.display << "\" " << b.tags << std::endl;
    std::cout << "info string bots " << g_bots.size() << std::endl;
}

void cmdWeightsFile(const std::string& path) {
    std::string err;
    if (load_weights_file(path, &err)) {
        g_weightsFile = path;
        syncWeightOptions();
        std::cout << "info string weights loaded from " << path << std::endl;
    } else {
        std::cout << "info string weights error: " << err << std::endl;
        return;
    }
    if (!err.empty())
        std::cout << "info string " << err << std::endl;
}

void handleButton(const std::string& name) {
    if (name == "Clear Hash") {
        TT.clear();
        std::cout << "info string hash cleared (" << TT.sizeMb() << " MB)" << std::endl;
    } else if (name == "ReloadWeights") {
        std::string file = g_weightsFile;
        if (file.empty())
            file = dataFile("weights_default.txt");
        if (file.empty())
            std::cout << "info string no weights file found" << std::endl;
        else
            cmdWeightsFile(file);
    } else if (name == "ResetWeights") {
        reset_weights();
        syncWeightOptions();
        std::cout << "info string weights reset to defaults" << std::endl;
    } else if (name == "ShowWeights") {
        std::cout << weights_report();
        std::cout << std::flush;
    } else if (name == "ListBots") {
        printBots();
    }
}

void cmdSetOption(std::istringstream& is) {
    std::string token, name, value;
    is >> token;  // "name"
    while (is >> token && token != "value")
        name += (name.empty() ? "" : " ") + token;
    while (is >> token)
        value += (value.empty() ? "" : " ") + token;

    Option* o = findOption(name);
    if (!o) {
        if (g_debug)
            std::cout << "info string unknown option: " << name << std::endl;
        return;
    }

    if (name == "Adaptive") {
        if (value != "true" && value != "false") {
            std::cout << "info string Adaptive expects true or false" << std::endl;
            return;
        }
        setAdaptive(value == "true");
        return;
    }
    if (g_adaptive) {
        std::cout << "info string Adaptive active: ignored option " << name << std::endl;
        return;
    }

    if (o->weightIndex >= 0 || name == "Style" || name == "Boldness" || name == "Bot"
        || name == "UCI_Elo" || name == "UCI_LimitStrength" || name == "WeightsFile"
        || name == "ReloadWeights" || name == "ResetWeights")
        TT.clear(); // cached scores and static evaluations belong to the previous profile

    if (o->type == "button") {
        handleButton(name);
        return;
    }

    o->value = value;

    if (o->weightIndex >= 0) {
        set_weight_index(o->weightIndex, std::atoi(value.c_str()));
    } else if (name == "Hash") {
        TT.resize(size_t(std::max(1, std::atoi(value.c_str()))));
        if (g_debug)
            std::cout << "info string hash " << TT.sizeMb() << " MB" << std::endl;
    } else if (name == "Threads") {
        setThreads(std::atoi(value.c_str()));
        std::cout << "info string threads " << threadCount() << std::endl;
    } else if (name == "WeightsFile") {
        if (value != "<empty>")
            cmdWeightsFile(value);
    } else if (name == "Bot") {
        uciSetBot(value);
    } else if (name == "Style") {
        const StyleDef* style = findStyle(value);
        if (style && applyStyle(value)) {
            syncWeightOptions();
            std::cout << "info string style " << style->name << " applied ("
                      << style->weights.size() << " weights set)" << std::endl;
        } else {
            std::cout << "info string unknown style: " << value << std::endl;
        }
    } else if (name == "Boldness") {
        applyBoldness(std::atoi(value.c_str()));
        syncWeightOptions();
        std::cout << "info string boldness " << value << " (10 = furchtsam ... 50 = stier)"
                  << std::endl;
    } else if (name == "UCI_Elo") {
        const int clamped = std::max(ELO_MIN, std::min(ELO_MAX, std::atoi(value.c_str())));
        const Option* limit = findOption("UCI_LimitStrength");
        if (limit && limit->value == "true") {
            applyElo(clamped);
            syncWeightOptions();
            const EloHandicap h = eloHandicap(clamped);
            std::cout << "info string elo " << clamped << " -> depth " << h.maxDepth << ", nodes "
                      << h.nodeLimit << ", eval noise " << h.moveError / 2 << ", move noise "
                      << h.moveNoise << ", blunder " << h.blunderPct << "%" << std::endl;
        } else if (g_debug) {
            std::cout << "info string UCI_Elo stored; set UCI_LimitStrength true to apply it"
                      << std::endl;
        }
    } else if (name == "UCI_LimitStrength") {
        const Option* elo = findOption("UCI_Elo");
        if (value == "true" && elo) {
            const int clamped = std::max(ELO_MIN, std::min(ELO_MAX, std::atoi(elo->value.c_str())));
            applyElo(clamped);
            syncWeightOptions();
            std::cout << "info string strength limited to " << clamped << " elo" << std::endl;
        } else {
            clearStrength();
            syncWeightOptions();
            std::cout << "info string full strength" << std::endl;
        }
    }
}

void cmdPosition(std::istringstream& is) {
    std::string token, fen;
    is >> token;
    if (token == "startpos") {
        fen = START_FEN;
        is >> token;
    } else if (token == "fen") {
        std::string part;
        for (int i = 0; i < 6 && (is >> part); ++i)
            fen += part + " ";
        is >> token;
    } else {
        return;
    }

    std::string err;
    if (!g_pos.setFen(fen, &err)) {
        std::cout << "info string invalid fen: " << err << std::endl;
        return;
    }
    g_gameKeys.clear();
    g_gameKeys.push_back(g_pos.key());  // include the initial position in threefold counting

    if (token == "moves") {
        std::string moveText;
        while (is >> moveText) {
            // Resolve against the legal move list instead of trusting the
            // parsed text: UCI writes castling as "e1g1" and en passant as an
            // ordinary pawn capture, so the flags of the generator are needed.
            // Without them the rook would stay behind and the board would
            // desynchronise (and the engine would answer with illegal moves).
            const Move m = find_legal_move(g_pos, moveText);
            if (m == 0) {
                std::cout << "info string illegal move: " << moveText << std::endl;
                break;
            }
            g_pos.applyMove(m);
            g_gameKeys.push_back(g_pos.key());
        }
    }
}

std::thread g_searchThread;

void joinSearch() {
    if (g_searchThread.joinable())
        g_searchThread.join();
}

void cmdGo(std::istringstream& is) {
    joinSearch();

    SearchLimits limits;
    std::string  token;
    while (is >> token) {
        if (token == "wtime")            is >> limits.wtime;
        else if (token == "btime")       is >> limits.btime;
        else if (token == "winc")        is >> limits.winc;
        else if (token == "binc")        is >> limits.binc;
        else if (token == "movestogo")   is >> limits.movestogo;
        else if (token == "depth")       is >> limits.depth;
        else if (token == "nodes")       is >> limits.nodes;
        else if (token == "movetime")    is >> limits.movetime;
        else if (token == "infinite")    limits.infinite = true;
        else if (token == "ponder")      limits.infinite = true;
        else if (token == "mate") { int ignored = 0; is >> ignored; }
    }

    if (g_adaptive) {
        limits.multiPV = TalParams::suche_kandidaten;
        if (!limits.infinite && limits.depth == 64 && !limits.nodes && !limits.movetime
            && limits.wtime <= 0 && limits.btime <= 0)
            limits.movetime = TalParams::suche_denkzeit_ms;
    }
    std::atomic<bool> started{false};
    g_searchThread = std::thread([limits, &started]() {
        SearchResult r = searchPosition(g_pos, limits, g_gameKeys, std::cout, &started);
        if (g_adaptive && r.best) applyAdaptiveChoice(r);
        std::cout << "bestmove " << (r.best ? move_to_uci(r.best) : "0000") << std::endl;
    });
    // Prevent a following stop/quit/setoption from being lost to the worker's
    // initial reset of g_stop. The worker never touches started again.
    while (!started.load(std::memory_order_acquire))
        std::this_thread::yield();
}

void cmdPerft(std::istringstream& is) {
    int depth = 1;
    is >> depth;
    if (depth < 1)
        depth = 1;

    const auto t0 = std::chrono::steady_clock::now();
    const uint64_t nodes = perftDivide(g_pos, depth, std::cout);
    const int64_t  ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0)
                            .count();
    std::cout << "Nodes: " << nodes << "  Time: " << ms << " ms  NPS: "
              << (ms > 0 ? nodes * 1000 / uint64_t(ms) : 0) << std::endl;
}

void cmdBench(std::istringstream& is) {
    int depth = 10;
    is >> depth;
    if (depth < 1)
        depth = 1;

    const auto t0 = std::chrono::steady_clock::now();
    const uint64_t nodes = benchNodes(depth);
    const int64_t  ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0)
                            .count();
    std::cout << "info string bench depth " << depth << " nodes " << nodes << " time " << ms
              << " ms nps " << (ms > 0 ? nodes * 1000 / uint64_t(ms) : 0) << std::endl;
}

void cmdEval() {
    const EvalBreakdown e = evaluateDetailed(g_pos);
    std::cout << "info string eval total " << e.total << " cp (material " << e.material << ", pst "
              << e.pst << ", tempo " << e.tempo << ", phase " << e.phase << ")" << std::endl;
    std::cout << "info string terms:";
    for (int t = 0; t < EVAL_TERM_NB; ++t)
        std::cout << " " << EVAL_TERM_NAMES[t] << "=" << e.term[t];
    std::cout << std::endl;
}

void cmdSanity() {
    std::string msg;
    if (g_pos.sanityCheck(&msg))
        std::cout << "info string sanity ok" << std::endl;
    else
        std::cout << "info string sanity FAILED: " << msg << std::endl;
}

void cmdCompiler() {
#if defined(_MSC_VER)
    std::cout << "info string compiler MSVC " << _MSC_VER << std::endl;
#elif defined(__clang__)
    std::cout << "info string compiler clang " << __clang_major__ << "." << __clang_minor__ << std::endl;
#elif defined(__GNUC__)
    std::cout << "info string compiler gcc " << __GNUC__ << "." << __GNUC_MINOR__ << std::endl;
#else
    std::cout << "info string compiler unknown" << std::endl;
#endif
#if defined(__ANDROID__)
    std::cout << "info string target android" << std::endl;
#elif defined(_WIN64)
    std::cout << "info string target windows-x64" << std::endl;
#elif defined(_WIN32)
    std::cout << "info string target windows-x86" << std::endl;
#else
    std::cout << "info string target posix" << std::endl;
#endif
    std::cout << "info string build " << __DATE__ << " " << __TIME__ << std::endl;
}

}  // namespace

void uciSetBot(const std::string& id) {
    g_currentBot = id;

    const StyleDef* style = styleFromBotId(id);
    if (!style) {
        std::cout << "info string bot '" << id << "' not found";
        if (!g_bots.empty())
            std::cout << " (" << g_bots.size() << " bots available, use the listbots command)";
        std::cout << std::endl;
        return;
    }

    const int         elo      = botEloFromId(id, 2800);
    const EloHandicap handicap = eloHandicap(elo);
    const BotInfo*    info     = findBot(id);

    applyStyle(style->id);
    applyElo(elo);
    syncWeightOptions();

    if (Option* option = findOption("UCI_Elo"))
        option->value = std::to_string(elo);
    if (Option* option = findOption("Style"))
        option->value = style->name;
    if (Option* option = findOption("UCI_LimitStrength"))
        option->value = "true";

    std::cout << "info string bot " << id << " elo " << elo << " style " << style->id
              << " name \"" << (info ? info->display : id) << "\"" << std::endl;
    std::cout << "info string applied: " << style->weights.size() << " style weights, depth "
              << handicap.maxDepth << ", nodes " << handicap.nodeLimit << ", eval noise "
              << handicap.moveError / 2 << " cp, move noise " << handicap.moveNoise
              << " cp, blunder " << handicap.blunderPct << "%" << std::endl;
}

void uciInit(int argc, char** argv) {
    (void)argc;
    if (argc > 0 && argv && argv[0]) {
        const std::string path = argv[0];
        const size_t      slash = path.find_last_of("/\\");
        g_exeDir = (slash == std::string::npos) ? "." : path.substr(0, slash);
    }

    registerOptions();

    std::string err;
    g_pos.setFen(START_FEN, &err);

    const std::string weights = dataFile("weights_default.txt");
    if (!weights.empty()) {
        std::string loadError;
        if (load_weights_file(weights, &loadError)) {
            g_weightsFile = weights;
            syncWeightOptions();
        }
    }

    loadBots();
}

bool uciCommandLine(int argc, char** argv) {
    if (argc < 2 || !argv)
        return false;

    const std::string arg = argv[1];

    if (arg == "--help" || arg == "-h") {
        std::cout << ENGINE_NAME << " - UCI chess engine\n\n"
                  << "usage: virgin [option]\n"
                  << "  (no option)                 start the UCI protocol loop\n"
                  << "  --perft <depth>             run perft from the start position\n"
                  << "  --bench <depth>             run the internal benchmark\n"
                  << "  --bot <id>                  preselect a bot profile (see data/bots.txt)\n"
                  << "  --list-bots                 print the bot catalog as UCI info lines\n"
                  << "  --weights                   print all weight values\n"
                  << "  --gen-data [dir]            write the data files (weights, styles, bots)\n"
                  << "  --selftest [dir]            run the built-in test suite\n"
                  << "  --selfplay <a> <b> <moves> <depth> [log]\n"
                  << "                              play a game between two bot profiles\n"
                  << "  --match <engine.exe> [startElo] [step] [games] [ms] [maxElo] [bot] [dir] [levels] [oppMs]\n"
                  << "                              play against another UCI engine and measure Elo\n"
                  << "  --tune [games] [depth] [passes] [outFile]\n"
                  << "                              Texel tuning: self play data, then fit the weights\n"
                  << std::endl;
        return true;
    }

    if (arg == "--perft") {
        const int depth = argc > 2 ? std::atoi(argv[2]) : 5;
        std::string err;
        g_pos.setFen(START_FEN, &err);
        const auto t0 = std::chrono::steady_clock::now();
        const uint64_t nodes = perft(g_pos, depth);
        const int64_t  ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - t0)
                                .count();
        std::cout << "perft(" << depth << ") = " << nodes << "  time " << ms << " ms  nps "
                  << (ms > 0 ? nodes * 1000 / uint64_t(ms) : 0) << std::endl;
        return true;
    }

    if (arg == "--bench") {
        const int depth = argc > 2 ? std::atoi(argv[2]) : 10;
        const auto t0 = std::chrono::steady_clock::now();
        const uint64_t nodes = benchNodes(depth);
        const int64_t  ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - t0)
                                .count();
        std::cout << "bench(" << depth << ") = " << nodes << " nodes  time " << ms << " ms  nps "
                  << (ms > 0 ? nodes * 1000 / uint64_t(ms) : 0) << std::endl;
        return true;
    }

    if (arg == "--list-bots") {
        printBots();
        return true;
    }

    if (arg == "--weights") {
        std::cout << weights_report();
        return true;
    }

    if (arg == "--bot") {
        if (argc > 2)
            uciSetBot(argv[2]);
        return true;
    }

    return false;
}

int uciLoop(int argc, char** argv) {
    (void)argc;
    (void)argv;

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream is(line);
        std::string token;
        if (!(is >> token))
            continue;

        if (token == "uci") {
            printUci();
        } else if (token == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (token == "ucinewgame") {
            stopSearch();
            joinSearch();
            newSearchGame();
            resetAdaptiveGame();
            g_gameKeys.clear();
        } else if (token == "setoption") {
            if (g_adaptive) {
                // Ignored options must not even interrupt the active search.
                std::istringstream preview(line);
                std::string word, name;
                preview >> word >> word; // setoption name
                while (preview >> word && word != "value")
                    name += (name.empty() ? "" : " ") + word;
                if (name != "Adaptive") {
                    std::cout << "info string Adaptive active: ignored option " << name << std::endl;
                    continue;
                }
            }
            stopSearch();
            joinSearch();
            cmdSetOption(is);
        } else if (token == "position") {
            joinSearch();
            cmdPosition(is);
        } else if (token == "go") {
            cmdGo(is);
        } else if (token == "stop") {
            stopSearch();
        } else if (token == "ponderhit") {
            stopSearch();
        } else if (token == "quit") {
            stopSearch();
            joinSearch();
            break;
        } else if (token == "debug") {
            std::string v;
            is >> v;
            g_debug = (v == "on");
        } else if (token == "d") {
            g_pos.print(std::cout);
        } else if (token == "eval") {
            cmdEval();
        } else if (token == "see") {
            joinSearch();
            std::string text;
            is >> text;
            const Move move = find_legal_move(g_pos, text);
            if (!move) {
                std::cout << "info string see invalid move" << std::endl;
            } else {
                int low = -SCORE_INFINITE, high = SCORE_INFINITE;
                while (low < high) {
                    const int mid = low + (high - low + 1) / 2;
                    if (seeGE(g_pos, move, mid)) low = mid;
                    else high = mid - 1;
                }
                std::cout << "info string see " << text << ' ' << low << " cp" << std::endl;
            }
        } else if (token == "perft") {
            cmdPerft(is);
        } else if (token == "bench") {
            cmdBench(is);
        } else if (token == "compiler") {
            cmdCompiler();
        } else if (token == "weights") {
            std::cout << weights_report();
        } else if (token == "listbots") {
            printBots();
        } else if (token == "sanity") {
            cmdSanity();
        } else {
            std::cout << "info string unknown command: " << token << std::endl;
        }
        std::cout.flush();
    }

    return 0;
}



