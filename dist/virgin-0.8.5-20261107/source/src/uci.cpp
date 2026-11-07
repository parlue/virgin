// Virgin - UCI chess engine
// UCI front end: options, position handling, go/perft/bench commands.
#include "uci.h"
#include "version.h"
#include "uci_output.h"
#include "embedded_config.h"
#ifdef VIRGIN_EMBEDDED
#include "embedded_worker.h"
#endif
#include "adaptive.h"
#include "live_performance.h"
#include <deque>
#include <mutex>
#include <optional>
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

std::ostream* g_output = &std::cout;
std::ostream& uciOutput() { return *g_output; }

#if defined(VIRGIN_CORE2)
constexpr const char* ENGINE_NAME = "Virgin 0.8.0 M5Stack Core2";
#elif defined(VIRGIN_EMBEDDED)
constexpr const char* ENGINE_NAME = "Virgin " VIRGIN_VERSION " ESP32-S3";
#else
constexpr const char* ENGINE_NAME   = "Virgin " VIRGIN_VERSION;
#endif
constexpr const char* ENGINE_AUTHOR = "Dirk D. Sommerfeld";

constexpr const char* START_FEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

Position    g_pos;
bool        g_debug = false;
#ifdef VIRGIN_EMBEDDED
constexpr int defaultMoveOverhead = 100;
#else
constexpr int defaultMoveOverhead = 50;
#endif
int g_moveOverhead = defaultMoveOverhead;
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
size_t g_savedHashKiB = 32768;
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
#ifdef VIRGIN_LIBRARY
    (void)name;
    return ""; // The APK library always starts from compiled-in defaults.
#else
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
#endif
}

void addOption(const Option& o) { g_options.push_back(o); }

void registerOptions() {
    g_options.clear();

#ifdef VIRGIN_EMBEDDED
    addOption({"HashKiB", "spin", std::to_string(VIRGIN_EMBEDDED_HASH_KIB),
               std::to_string(VIRGIN_EMBEDDED_HASH_KIB), 16, VIRGIN_EMBEDDED_HASH_KIB, -1, {}});
#else
#ifdef VIRGIN_WASM
    addOption({"Hash", "spin", "32", "32", 1, 256, -1, {}});
#else
    addOption({"Hash", "spin", "32", "32", 1, 4096, -1, {}});
#endif
#endif
    // 0 = automatic: as many threads as there are cores, capped at 4 (the
    // measured sweet spot). The default shown is the resolved value.
    addOption({"Threads", "spin", std::to_string(threadCount()).c_str(),
               std::to_string(threadCount()).c_str(), 0,
#ifdef VIRGIN_EMBEDDED
               1,
#elif defined(VIRGIN_WASM)
               4,
#else
               64,
#endif
               -1, {}});
#ifndef VIRGIN_CORE2
    addOption({"MultiPV", "spin", "1", "1", 1, 5, -1, {}});
#endif
    addOption({"Adaptive", "check", "false", "false", 0, 0, -1, {}});
    addOption({"LivePerformance", "check", "false", "false", 0, 0, -1, {}});
    addOption({"Ponder", "check", "false", "false", 0, 0, -1, {}});
#ifndef VIRGIN_CORE2
    addOption({"Move Overhead", "spin", std::to_string(defaultMoveOverhead),
               std::to_string(g_moveOverhead), 0, 5000, -1, {}});
#endif
    addOption({"UCI_LimitStrength", "check", "false", "false", 0, 0, -1, {}});
    addOption({"UCI_Elo", "spin", "2800", "2800", 700, 3000, -1, {}});
    addOption({"Style", "combo", "Normal", "Normal", 0, 0, -1,
               {"Fortress", "Stone", "Normal", "Fire", "Thunder"}});
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

#include "uci_performance.inc"

void printUci() {
    const UciOutputGuard outputGuard;
    uciOutput() << "id name " << ENGINE_NAME << "\n";
    uciOutput() << "id author " << ENGINE_AUTHOR << "\n";
    for (const Option& o : g_options) {
        if (o.type == "button") {
            uciOutput() << "option name " << o.name << " type button\n";
            continue;
        }
        uciOutput() << "option name " << o.name << " type " << o.type << " default " << o.def;
        if (o.type == "spin")
            uciOutput() << " min " << o.min << " max " << o.max;
        if (o.type == "combo")
            for (const std::string& v : o.vars)
                uciOutput() << " var " << v;
        uciOutput() << "\n";
    }
    uciOutput() << "uciok" << std::endl;
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
        g_savedHashKiB = TT.entries() * sizeof(TTEntry) / 1024;
        applyStyle("Fire");
        clearStrength(); setThreads(0);
#ifdef VIRGIN_EMBEDDED
        TT.resizeKiB(VIRGIN_EMBEDDED_HASH_KIB);
#else
        TT.resize(TalParams::suche_hash_mb);
#endif
        g_adaptive = true;
        findOption("Adaptive")->value = "true";
        syncWeightOptions();
        uciOutput() << "info string Adaptive on: full strength, five candidates, fixed attack profile, hash "
                  << TT.entries() * sizeof(TTEntry) / 1024 << " KiB, threads " << threadCount() << std::endl;
    } else {
        for (int i = 0; i < weight_count(); ++i) set_weight_index(i, g_savedWeights[size_t(i)]);
        std::copy(g_savedKnowledge.begin(), g_savedKnowledge.end(), EW.knowledge);
        g_strength = g_savedStrength;
        setThreads(g_savedThreads); TT.resizeKiB(g_savedHashKiB);
        g_options = g_savedOptions;
        g_adaptive = false;
        uciOutput() << "info string Adaptive off: previous settings restored" << std::endl;
    }
    resetAdaptiveGame(); newSearchGame(); resetPerformance();
}

void applyAdaptiveChoice(SearchResult& result) {
    if (result.candidates.empty()) {
        uciOutput() << "info string adaptive incomplete: using legal full-strength fallback" << std::endl;
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
    auto choice = chooseAdaptive(g_pos, result.candidates, ply, g_adaptiveState,
                                 g_adaptiveRoll, g_adaptivePick);
#ifndef VIRGIN_CORE2
    if (choice.mode == "eroeffnung" && result.openingVariety) {
        choice.index = chooseOpeningCandidate(g_pos,result,g_adaptivePick);
        if (choice.index) {
            choice.mode = "eroeffnung_variante";
            choice.afterEval = -result.candidates[choice.index].score;
            g_adaptiveState.expected = choice.afterEval;
        }
    }
#endif
    const auto& candidate = result.candidates[choice.index];
    const Move strongest = result.candidates.front().move;
    result.best = candidate.move; result.score = candidate.score; result.pv = candidate.pv;
    Position chosen = g_pos; chosen.applyMove(result.best); g_adaptiveChosenKey = chosen.key();
    uciOutput() << "info string adaptive " << choice.mode << " player " << choice.playerEval
              << " after " << choice.afterEval << " accuracy " << choice.accuracy
              << " best " << move_to_uci(strongest) << " selected " << move_to_uci(result.best)
              << " samples " << g_adaptiveState.accuracy.size() << " helped " << g_adaptiveState.helped << " adjusts " << g_adaptiveState.adjusts << std::endl;
    uciOutput() << "info depth " << result.depth << " score cp " << result.score
              << " nodes " << result.nodes << " time " << result.timeMs << " pv";
    for (Move move : result.pv) uciOutput() << ' ' << move_to_uci(move);
    uciOutput() << std::endl;
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
    const UciOutputGuard outputGuard;
    if (g_bots.empty()) {
        uciOutput() << "info string no bot catalog found (expected data/bots.txt next to the binary)"
                  << std::endl;
        return;
    }
    for (const BotInfo& b : g_bots)
        uciOutput() << "info string bot " << b.id << " " << b.elo << " " << b.style << " \""
                  << b.display << "\" " << b.tags << std::endl;
    uciOutput() << "info string bots " << g_bots.size() << std::endl;
}

void cmdWeightsFile(const std::string& path) {
    std::string err;
    if (load_weights_file(path, &err)) {
        g_weightsFile = path;
        syncWeightOptions();
        uciOutput() << "info string weights loaded from " << path << std::endl;
    } else {
        uciOutput() << "info string weights error: " << err << std::endl;
        return;
    }
    if (!err.empty())
        uciOutput() << "info string " << err << std::endl;
}

void handleButton(const std::string& name) {
    if (name == "Clear Hash") {
        TT.clear();
        uciOutput() << "info string hash cleared (" << TT.sizeMb() << " MB)" << std::endl;
    } else if (name == "ReloadWeights") {
        std::string file = g_weightsFile;
        if (file.empty())
            file = dataFile("weights_default.txt");
        if (file.empty())
            uciOutput() << "info string no weights file found" << std::endl;
        else
            cmdWeightsFile(file);
    } else if (name == "ResetWeights") {
        reset_weights();
        syncWeightOptions();
        uciOutput() << "info string weights reset to defaults" << std::endl;
    } else if (name == "ShowWeights") {
        uciOutput() << weights_report();
        uciOutput() << std::flush;
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
            uciOutput() << "info string unknown option: " << name << std::endl;
        return;
    }

#ifndef VIRGIN_CORE2
    if (name == "MultiPV") {
        o->value=std::to_string(std::clamp(std::atoi(value.c_str()),1,5));
        return;
    }
#endif
    if (name == "LivePerformance") {
        if(value!="true" && value!="false") { uciOutput()<<"info string LivePerformance expects true or false"<<std::endl; return; }
        const bool previous=performanceActive();g_livePerformance=value=="true";o->value=value;
        for(auto& saved:g_savedOptions)if(saved.name==name)saved.value=value;
        if(previous!=performanceActive())resetPerformance();
        return;
    }
    if (name == "Adaptive") {
        if (value != "true" && value != "false") {
            uciOutput() << "info string Adaptive expects true or false" << std::endl;
            return;
        }
        setAdaptive(value == "true");
        return;
    }
#ifndef VIRGIN_CORE2
    if (name == "Move Overhead") {
        g_moveOverhead = std::clamp(std::atoi(value.c_str()),0,5000);
        o->value = std::to_string(g_moveOverhead);
        for (auto& saved : g_savedOptions) if (saved.name==name) saved.value=o->value;
        return;
    }
#endif
    if (g_adaptive) {
        uciOutput() << "info string Adaptive active: ignored option " << name << std::endl;
        return;
    }

    if (o->weightIndex >= 0 || name == "Style" || name == "Boldness" || name == "Bot"
        || name == "UCI_Elo" || name == "UCI_LimitStrength" || name == "WeightsFile"
        || name == "ReloadWeights" || name == "ResetWeights")
        { TT.clear(); restartPerformanceProfile(); } // remeasure the current history under the new profile

    if (o->type == "button") {
        handleButton(name);
        return;
    }

    o->value = value;

    if (o->weightIndex >= 0) {
        set_weight_index(o->weightIndex, std::atoi(value.c_str()));
    } else if (name == "HashKiB") {
#ifdef VIRGIN_EMBEDDED
        TT.resizeKiB(size_t(std::clamp(std::atoi(value.c_str()), 16, VIRGIN_EMBEDDED_HASH_KIB)));
#endif
    } else if (name == "Hash") {
#ifdef VIRGIN_WASM
        TT.resize(size_t(std::clamp(std::atoi(value.c_str()), 1, 256)));
#else
        TT.resize(size_t(std::max(1, std::atoi(value.c_str()))));
#endif
        if (g_debug)
            uciOutput() << "info string hash " << TT.sizeMb() << " MB" << std::endl;
    } else if (name == "Threads") {
        setThreads(std::atoi(value.c_str()));
        uciOutput() << "info string threads " << threadCount() << std::endl;
    } else if (name == "WeightsFile") {
        if (value != "<empty>")
            cmdWeightsFile(value);
    } else if (name == "Bot") {
        uciSetBot(value);
    } else if (name == "Style") {
        const StyleDef* style = findStyle(value);
        if (style && applyStyle(value)) {
            o->value = style->name;
            syncWeightOptions();
            uciOutput() << "info string style " << style->name << " applied ("
                      << style->weights.size() << " weights set)" << std::endl;
        } else {
            uciOutput() << "info string unknown style: " << value << std::endl;
        }
    } else if (name == "Boldness") {
        applyBoldness(std::atoi(value.c_str()));
        syncWeightOptions();
        uciOutput() << "info string boldness " << value << " (10 = Fortress ... 50 = Thunder)"
                  << std::endl;
    } else if (name == "UCI_Elo") {
        const int clamped = std::max(ELO_MIN, std::min(ELO_MAX, std::atoi(value.c_str())));
        const Option* limit = findOption("UCI_LimitStrength");
        if (limit && limit->value == "true") {
            applyElo(clamped);
            syncWeightOptions();
            const EloHandicap h = eloHandicap(clamped);
            uciOutput() << "info string elo " << clamped << " -> depth " << h.maxDepth << ", nodes "
                      << h.nodeLimit << ", eval noise " << h.moveError / 2 << ", move noise "
                      << h.moveNoise << ", blunder " << h.blunderPct << "%" << std::endl;
        } else if (g_debug) {
            uciOutput() << "info string UCI_Elo stored; set UCI_LimitStrength true to apply it"
                      << std::endl;
        }
    } else if (name == "UCI_LimitStrength") {
        const Option* elo = findOption("UCI_Elo");
        if (value == "true" && elo) {
            const int clamped = std::max(ELO_MIN, std::min(ELO_MAX, std::atoi(elo->value.c_str())));
            applyElo(clamped);
            syncWeightOptions();
            uciOutput() << "info string strength limited to " << clamped << " elo" << std::endl;
        } else {
            clearStrength();
            syncWeightOptions();
            uciOutput() << "info string full strength" << std::endl;
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
        uciOutput() << "info string invalid fen: " << err << std::endl;
        return;
    }
    const Position performanceBase=g_pos;
    std::vector<Move> performanceMoves;
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
                uciOutput() << "info string illegal move: " << moveText << std::endl;
                break;
            }
            performanceMoves.push_back(m);
            g_pos.applyMove(m);
            g_gameKeys.push_back(g_pos.key());
        }
    }
    stagePerformance(performanceBase,performanceMoves);
}

#ifdef VIRGIN_EMBEDDED
EmbeddedWorker g_embeddedWorker;
#else
std::thread g_searchThread;
#endif

void joinSearch() {
#ifdef VIRGIN_EMBEDDED
    g_embeddedWorker.wait();
#else
    if (g_searchThread.joinable())
        g_searchThread.join();
#endif
}

void cmdGo(std::istringstream& is) {
    joinSearch();

    SearchLimits limits;
    bool explicitLimit = false, speculativePonder = false;
    const bool prepare=g_prepareNext;g_prepareNext=false;
    g_commandCancel.store(false);
    limits.cancellation=&g_commandCancel;
    std::string  token;
    while (is >> token) {
        if (token == "wtime" || token == "btime" || token == "depth" || token == "nodes"
            || token == "movetime" || token == "mate") explicitLimit = true;
#ifndef VIRGIN_CORE2
        if (token == "searchmoves") {
            limits.restrictRootMoves=true;
            for (;;) {
                const auto cursor=is.tellg(); std::string text;
                if (!(is>>text)) break;
                const std::array<const char*,12> keywords={"wtime","btime","winc","binc","movestogo",
                    "depth","nodes","movetime","mate","infinite","ponder","searchmoves"};
                if (std::any_of(keywords.begin(),keywords.end(),[&](const char* k){return text==k;})) {
                    is.clear();is.seekg(cursor);break;
                }
                const Move m=find_legal_move(g_pos,text);
                if (m && std::find(limits.rootMoves.begin(),limits.rootMoves.end(),m)==limits.rootMoves.end())
                    limits.rootMoves.push_back(m);
                else if (!m) uciOutput()<<"info string ignored illegal searchmove "<<text<<std::endl;
            }
        }
        else
#endif
        if (token == "wtime") { is >> limits.wtime; limits.wtime=std::max<int64_t>(0,limits.wtime); }
        else if (token == "btime") { is >> limits.btime; limits.btime=std::max<int64_t>(0,limits.btime); }
        else if (token == "winc")        is >> limits.winc;
        else if (token == "binc")        is >> limits.binc;
        else if (token == "movestogo")   is >> limits.movestogo;
        else if (token == "depth")       is >> limits.depth;
        else if (token == "nodes") { is >> limits.nodes; limits.nodes=std::max<uint64_t>(1,limits.nodes); }
        else if (token == "movetime") { is >> limits.movetime; limits.movetime=std::max<int64_t>(1,limits.movetime); }
        else if (token == "infinite")    limits.infinite = true;
        else if (token == "ponder") { limits.infinite = true; speculativePonder=true; }
        else if (token == "mate") { int ignored = 0; is >> ignored; }
    }

#ifndef VIRGIN_CORE2
    limits.multiPV=std::clamp(std::atoi(findOption("MultiPV")->value.c_str()),1,5);
#endif
    if (g_adaptive) limits.multiPV = TalParams::suche_kandidaten;
#ifdef VIRGIN_CORE2
    if (!limits.infinite && !explicitLimit)
        limits.movetime = g_adaptive ? TalParams::suche_denkzeit_ms : 1000;
#else
    if (!limits.infinite && !explicitLimit) limits.automaticTime = g_adaptive ? 10000 : 5000;
    limits.moveOverhead = g_moveOverhead;
    limits.depth = std::max(1,limits.depth);
    const bool timed = limits.movetime>0 || limits.automaticTime>0 ||
        (g_pos.sideToMove()==WHITE ? limits.wtime>=0 : limits.btime>=0);
    limits.openingVariety = timed && !limits.infinite && !limits.restrictRootMoves &&
        (g_adaptive || limits.multiPV==1) && g_pos.fullmoveNumber()<=4
        && (g_adaptive || findOption("UCI_LimitStrength")->value != "true");
    if (limits.openingVariety && !g_adaptive) limits.multiPV = 3;
#endif
    std::atomic<bool> started{false};
#ifdef VIRGIN_EMBEDDED
    g_embeddedWorker.start([limits, prepare, speculativePonder, &started]() mutable {
#else
    g_searchThread = std::thread([limits, prepare, speculativePonder, &started]() mutable {
#endif
        started.store(true,std::memory_order_release);
        const auto began=std::chrono::steady_clock::now();
        int budget=100;
        if(limits.movetime>0)budget=int(std::min<int64_t>(100,limits.movetime/10));
        else {const auto clock=g_pos.sideToMove()==WHITE?limits.wtime:limits.btime;if(clock>=0)budget=int(std::min<int64_t>(100,clock/100));}
        if(budget>=8)measurePerformance(budget,speculativePonder);
        const int64_t spent=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-began).count();
        if(limits.movetime>0)limits.movetime=std::max<int64_t>(1,limits.movetime-spent);
        if(limits.automaticTime>0)limits.automaticTime=std::max<int>(1,limits.automaticTime-int(spent));
        if(g_pos.sideToMove()==WHITE && limits.wtime>=0)limits.wtime=std::max<int64_t>(0,limits.wtime-spent);
        if(g_pos.sideToMove()==BLACK && limits.btime>=0)limits.btime=std::max<int64_t>(0,limits.btime-spent);
        if(prepare) {
            const Strength saved=g_strength;const int threads=threadCount();std::array<int,GROUP_NB> knowledge;
            std::copy_n(EW.knowledge,GROUP_NB,knowledge.begin());const bool changed=!fullPerformanceProfile();
            clearStrength();setThreads(1);if(changed)TT.clear();
            PerformanceSink buffer;std::ostream sink(&buffer);limits.multiPV=3;limits.openingVariety=false;limits.depth=12;
            const auto prepared=searchPosition(g_pos,limits,g_gameKeys,sink);cachePerformance(g_pos,prepared);
            g_strength=saved;std::copy(knowledge.begin(),knowledge.end(),EW.knowledge);setThreads(threads);if(changed)TT.clear();
            const UciOutputGuard guard;uciOutput()<<"info string performance prepared depth "<<prepared.depth<<std::endl;
            return;
        }
        SearchResult r = searchPosition(g_pos, limits, g_gameKeys, uciOutput());
        cachePerformance(g_pos,r);
        const UciOutputGuard outputGuard;
        if (g_adaptive && r.best) applyAdaptiveChoice(r);
#ifndef VIRGIN_CORE2
        else if (r.best && limits.openingVariety) {
            std::uniform_real_distribution<double> random(0.0,1.0);
            const size_t index = chooseOpeningCandidate(g_pos,r,random(g_adaptiveRng));
            if (index) {
                const auto& c=r.candidates[index];
                uciOutput() << "info string opening near-equal loss " << r.candidates[0].score-c.score
                            << " cp selected " << move_to_uci(c.move) << std::endl;
                r.best=c.move; r.score=c.score; r.pv=c.pv;
            }
            // End with the actually selected primary PV, even when the best
            // candidate is retained. Simple hosts must not retain rank 3's score.
            uciOutput() << "info depth " << r.depth << " score cp " << r.score
                        << " nodes " << r.nodes << " time " << r.timeMs << " pv";
            for (Move move:r.pv) uciOutput() << ' ' << move_to_uci(move);
            uciOutput() << std::endl;
        }
#endif
        uciOutput() << "bestmove " << (r.best ? move_to_uci(r.best) : "0000") << std::endl;
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
#ifdef VIRGIN_EMBEDDED
    depth = std::min(depth, 5); // diagnostic runs on the small console stack
#endif

    const auto t0 = std::chrono::steady_clock::now();
    const uint64_t nodes = perftDivide(g_pos, depth, uciOutput());
    const int64_t  ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0)
                            .count();
    uciOutput() << "Nodes: " << nodes << "  Time: " << ms << " ms  NPS: "
              << (ms > 0 ? nodes * 1000 / uint64_t(ms) : 0) << std::endl;
}

[[maybe_unused]] void cmdBench(std::istringstream& is) {
    int depth = 10;
    is >> depth;
    if (depth < 1)
        depth = 1;
#ifdef VIRGIN_EMBEDDED
    depth = std::min(depth, 5); // diagnostic runs on the small console stack
#endif

    const auto t0 = std::chrono::steady_clock::now();
    const uint64_t nodes = benchNodes(depth);
    const int64_t  ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0)
                            .count();
    uciOutput() << "info string bench depth " << depth << " nodes " << nodes << " time " << ms
              << " ms nps " << (ms > 0 ? nodes * 1000 / uint64_t(ms) : 0) << std::endl;
}

void cmdEval() {
    const UciOutputGuard outputGuard;
    const EvalBreakdown e = evaluateDetailed(g_pos);
    uciOutput() << "info string eval total " << e.total << " cp (material " << e.material << ", pst "
              << e.pst << ", tempo " << e.tempo << ", phase " << e.phase << ")" << std::endl;
    uciOutput() << "info string terms:";
    for (int t = 0; t < EVAL_TERM_NB; ++t)
        uciOutput() << " " << EVAL_TERM_NAMES[t] << "=" << e.term[t];
    uciOutput() << std::endl;
}

void cmdSanity() {
    const UciOutputGuard outputGuard;
    std::string msg;
    if (g_pos.sanityCheck(&msg))
        uciOutput() << "info string sanity ok" << std::endl;
    else
        uciOutput() << "info string sanity FAILED: " << msg << std::endl;
}

void cmdCompiler() {
    const UciOutputGuard outputGuard;
#if defined(_MSC_VER)
    uciOutput() << "info string compiler MSVC " << _MSC_VER << std::endl;
#elif defined(__clang__)
    uciOutput() << "info string compiler clang " << __clang_major__ << "." << __clang_minor__ << std::endl;
#elif defined(__GNUC__)
    uciOutput() << "info string compiler gcc " << __GNUC__ << "." << __GNUC_MINOR__ << std::endl;
#else
    uciOutput() << "info string compiler unknown" << std::endl;
#endif
#if defined(__ANDROID__)
    uciOutput() << "info string target android" << std::endl;
#elif defined(_WIN64)
    uciOutput() << "info string target windows-x64" << std::endl;
#elif defined(_WIN32)
    uciOutput() << "info string target windows-x86" << std::endl;
#else
    uciOutput() << "info string target posix" << std::endl;
#endif
    uciOutput() << "info string build " << __DATE__ << " " << __TIME__ << std::endl;
}

}  // namespace

void uciSetBot(const std::string& id) {
    g_currentBot = id;

    const StyleDef* style = styleFromBotId(id);
    if (!style) {
        uciOutput() << "info string bot '" << id << "' not found";
        if (!g_bots.empty())
            uciOutput() << " (" << g_bots.size() << " bots available, use the listbots command)";
        uciOutput() << std::endl;
        return;
    }

    const int         elo      = botEloFromId(id, 2800);
    const EloHandicap handicap = eloHandicap(elo);
    const BotInfo*    info     = findBot(botId(elo, style->id));
    if (!info) info = findBot(id);

    applyStyle(style->id);
    applyElo(elo);
    syncWeightOptions();

    if (Option* option = findOption("UCI_Elo"))
        option->value = std::to_string(elo);
    if (Option* option = findOption("Style"))
        option->value = style->name;
    if (Option* option = findOption("UCI_LimitStrength"))
        option->value = "true";

    uciOutput() << "info string bot " << id << " elo " << elo << " style " << style->id
              << " name \"" << (info ? info->display : id) << "\"" << std::endl;
    uciOutput() << "info string applied: " << style->weights.size() << " style weights, depth "
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
#ifdef VIRGIN_EMBEDDED
    g_embeddedWorker.start([] {}); // reserve stack before announcing readiness
    g_embeddedWorker.wait();
#endif
}

bool uciCommandLine(int argc, char** argv) {
    if (argc < 2 || !argv)
        return false;

    const std::string arg = argv[1];

    if (arg == "--help" || arg == "-h") {
        uciOutput() << ENGINE_NAME << " - UCI chess engine\n\n"
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
        uciOutput() << "perft(" << depth << ") = " << nodes << "  time " << ms << " ms  nps "
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
        uciOutput() << "bench(" << depth << ") = " << nodes << " nodes  time " << ms << " ms  nps "
                  << (ms > 0 ? nodes * 1000 / uint64_t(ms) : 0) << std::endl;
        return true;
    }

    if (arg == "--list-bots") {
        printBots();
        return true;
    }

    if (arg == "--weights") {
        uciOutput() << weights_report();
        return true;
    }

    if (arg == "--bot") {
        if (argc > 2)
            uciSetBot(argv[2]);
        return true;
    }

    return false;
}

void uciShutdown() { g_commandCancel.store(true); stopSearch(); joinSearch(); }

void uciResetSession() {
    uciShutdown();
    g_adaptive = false; g_debug = false;g_livePerformance=false;resetPerformance();
    g_savedOptions.clear(); g_savedWeights.clear();
    g_weightsFile.clear(); g_currentBot.clear(); g_gameKeys.clear();
    g_moveOverhead = defaultMoveOverhead;
    reset_weights(); clearStrength(); setThreads(0); TT.resize(32);
    newSearchGame(); resetAdaptiveGame();
    uciInit(0, nullptr);
}

int uciLoop(int argc, char** argv) {
    (void)argc; (void)argv;
    return uciRun(std::cin, std::cout);
}

int uciRun(std::istream& input, std::ostream& output) {
    struct SessionGuard {
        std::ostream* previous;
        ~SessionGuard() { uciShutdown(); g_output = previous; }
    } guard{g_output};
    g_output = &output;

    std::string line;
    while (std::getline(input, line)) {
        std::istringstream is(line);
        std::string token;
        if (!(is >> token))
            continue;

        if (token == "uci") {
            printUci();
        } else if (token == "isready") {
            const UciOutputGuard outputGuard;
            uciOutput() << "readyok" << std::endl;
        } else if (token == "ucinewgame") {
            g_commandCancel.store(true);stopSearch();
            joinSearch();
            newSearchGame();
            resetAdaptiveGame();resetPerformance();
            g_gameKeys.clear();
        } else if (token == "setoption") {
            if (g_adaptive) {
                // Ignored options must not even interrupt the active search.
                std::istringstream preview(line);
                std::string word, name;
                preview >> word >> word; // setoption name
                while (preview >> word && word != "value")
                    name += (name.empty() ? "" : " ") + word;
                if (name != "Adaptive" && name != "LivePerformance"
#ifndef VIRGIN_CORE2
                    && name != "Move Overhead"
#endif
                   ) {
                    const UciOutputGuard outputGuard;
                    uciOutput() << "info string Adaptive active: ignored option " << name << std::endl;
                    continue;
                }
            }
            g_commandCancel.store(true);stopSearch();
            joinSearch();
            cmdSetOption(is);
        } else if (token == "position") {
            joinSearch();
            cmdPosition(is);
        } else if (token == "go") {
            cmdGo(is);
        } else if (token == "stop") {
            g_commandCancel.store(true);stopSearch();
        } else if (token == "ponderhit") {
            g_commandCancel.store(true);stopSearch();
        } else if (token == "quit") {
            g_commandCancel.store(true);stopSearch();
            joinSearch();
            break;
        } else if (token == "performance") {
            std::string action;is>>action;
            if(action=="prepare" && performanceActive()) {
                g_commandCancel.store(true);stopSearch();joinSearch();
                g_prepareNext=true;std::istringstream request("movetime 250");cmdGo(request);
            } else reportPerformance();
        } else if (token == "debug") {
            std::string v;
            is >> v;
            g_debug = (v == "on");
        } else if (token == "d") {
            const UciOutputGuard outputGuard;
            g_pos.print(uciOutput());
        } else if (token == "eval") {
            cmdEval();
        } else if (token == "see") {
            joinSearch();
            std::string text;
            is >> text;
            const Move move = find_legal_move(g_pos, text);
            if (!move) {
                uciOutput() << "info string see invalid move" << std::endl;
            } else {
                int low = -SCORE_INFINITE, high = SCORE_INFINITE;
                while (low < high) {
                    const int mid = low + (high - low + 1) / 2;
                    if (seeGE(g_pos, move, mid)) low = mid;
                    else high = mid - 1;
                }
                uciOutput() << "info string see " << text << ' ' << low << " cp" << std::endl;
            }
        } else if (token == "perft") {
            cmdPerft(is);
        } else if (token == "bench") {
#ifdef VIRGIN_EMBEDDED
            uciOutput() << "info string use go depth for embedded search diagnostics" << std::endl;
#else
            cmdBench(is);
#endif
        } else if (token == "compiler") {
            cmdCompiler();
        } else if (token == "weights") {
            const UciOutputGuard outputGuard;
            uciOutput() << weights_report();
        } else if (token == "listbots") {
            printBots();
        } else if (token == "sanity") {
            cmdSanity();
        } else {
            uciOutput() << "info string unknown command: " << token << std::endl;
        }
        uciOutput().flush();
    }

    return 0;
}



