// Virgin - UCI chess engine
// Texel style tuning: self play positions with game results, then a logistic
// regression (K scan + coordinate descent) over the registered weights.
#pragma once

#include <string>

struct TuneConfig {
    int         games = 120;      // self play games for the data set
    int         depth = 6;        // search depth while collecting
    int         maxPasses = 5;    // coordinate descent passes
    int         maxPositions = 60000;
    int         maxDeltaPercent = 20;  // per parameter limit vs its default (%)
    int         testPercent = 20;      // share of positions kept for validation
    std::string out = "data/weights_tuned.txt";
    std::string dataDir = "data";  // where the report is written
};

// Runs the tuner. Returns 0 on success.
int runTune(const TuneConfig& config);
