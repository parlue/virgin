// Virgin - UCI chess engine
// Match mode: play a series of games against an external UCI engine
// (e.g. Stockfish with UCI_LimitStrength) and estimate our own Elo.
#pragma once

#include <string>

struct MatchConfig {
    std::string opponent;                        // path to the opponent engine
    std::string ourBot = "v2800-ausgeglichen";   // our bot profile
    int         startElo = 2300;                 // opponent UCI_Elo to start with
    int         step = 50;                       // increase per level
    int         maxElo = 2800;                   // last level
    int         gamesPerLevel = 4;               // 2 as white, 2 as black
    int         movetimeMs = 1000;               // time per move for us
    int         opponentMovetimeMs = 0;          // 0 = same as movetimeMs
    int    maxMoves = 220;                   // halfmoves before adjudication
    std::string logDir = "reports";
    int    maxLevels = 12;                   // levels played by the Elo search
    bool   ascendingOnly = false;            // pure upward ladder (no descent)
    std::string weightsFile;                 // optional: load after style/Elo
    int         ourThreads = 0;              // Lazy SMP threads for our side (0 = auto)
};

// Runs the match. Returns 0 on success.
int runMatch(const MatchConfig& config);
