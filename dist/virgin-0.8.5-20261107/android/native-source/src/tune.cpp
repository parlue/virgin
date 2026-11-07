// Virgin - UCI chess engine
// Texel tuning: play self play games, keep quiet positions together with the
// game result and fit the evaluation weights so that sigmoid(eval / K) matches
// the result as closely as possible.
#include "tune.h"

#include "eval.h"
#include "movegen.h"
#include "position.h"
#include "search.h"
#include "weights.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr const char* START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

struct Sample {
    Position pos;
    double   result = 0.5;  // game result from White's point of view
};

// The same short openings the match mode uses, so the data set is varied.
const std::vector<std::vector<const char*>>& tuneOpenings() {
    static const std::vector<std::vector<const char*>> list = {
        {"e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "a7a6"},  // Ruy Lopez
        {"e2e4", "c7c5", "g1f3", "d7d6", "d2d4", "c5d4"},  // Sicilian
        {"d2d4", "d7d5", "c2c4", "e7e6", "b1c3", "g8f6"},  // Queen's Gambit
        {"d2d4", "g8f6", "c2c4", "g7g6", "b1c3", "f8g7"},  // King's Indian
        {"e2e4", "e7e6", "d2d4", "d7d5", "b1c3", "g8f6"},  // French
        {"e2e4", "c7c6", "d2d4", "d7d5", "b1c3", "d5e4"},  // Caro-Kann
        {"g1f3", "d7d5", "d2d4", "g8f6", "c2c4", "e7e6"},  // Reti
        {"e2e4", "d7d5", "e4d5", "d8d5", "b1c3", "d5a5"},  // Scandinavian
    };
    return list;
}

double sigmoid(double x) {
    if (x > 40.0)  return 1.0;
    if (x < -40.0) return 0.0;
    return 1.0 / (1.0 + std::exp(-x));
}

// Static evaluation of the position from White's point of view, in centipawns.
double whiteEval(const Position& pos) {
    const double s = double(evaluate(pos));
    return pos.sideToMove() == WHITE ? s : -s;
}

// Mean squared error between sigmoid(eval / k) and the game results.
double meanError(const std::vector<Sample>& samples, double k) {
    double sum = 0.0;
    for (const Sample& s : samples) {
        const double diff = sigmoid(whiteEval(s.pos) / k) - s.result;
        sum += diff * diff;
    }
    return samples.empty() ? 0.0 : sum / double(samples.size());
}

// Plays `games` self play games and keeps quiet positions with the result.
void collectSamples(const TuneConfig& cfg, std::vector<Sample>& samples, std::ostream& out) {
    samples.clear();
    samples.reserve(size_t(cfg.maxPositions));
    int whiteWins = 0, blackWins = 0, draws = 0;

    for (int g = 0; g < cfg.games; ++g) {
        newSearchGame();
        Position    pos;
        std::string err;
        pos.setFen(START_FEN, &err);
        std::vector<Key> keys{pos.key()};

        const auto& opening = tuneOpenings()[size_t(g / 2) % tuneOpenings().size()];
        for (const char* text : opening) {
            bool       ok = false;
            const Move m  = parse_uci_move(text, ok);
            if (!ok || !pos.isLegal(m))
                break;
            pos.applyMove(m);
            keys.push_back(pos.key());
        }

        std::vector<Position> gamePositions;
        int                   whiteScore = -1;  // -1 = draw
        for (int ply = 0; ply < 220; ++ply) {
            MoveList legal;
            if (generateLegal(pos, legal) == 0) {
                whiteScore = pos.inCheck() ? (pos.sideToMove() == WHITE ? 0 : 1) : -1;
                break;
            }
            if (pos.halfmoveClock() >= 100)
                break;
            if (!pos.inCheck() && (ply % 3 == 0))
                gamePositions.push_back(pos);

            SearchLimits       limits;
            limits.depth = cfg.depth;
            std::ostringstream sink;
            const SearchResult r = searchPosition(pos, limits, keys, sink);
            if (r.best == 0)
                break;
            pos.applyMove(r.best);
            keys.push_back(pos.key());
        }

        const double result = (whiteScore == 1) ? 1.0 : (whiteScore == 0 ? 0.0 : 0.5);
        if (whiteScore == 1)      ++whiteWins;
        else if (whiteScore == 0) ++blackWins;
        else                      ++draws;

        for (const Position& p : gamePositions) {
            if (samples.size() >= size_t(cfg.maxPositions))
                break;
            Sample s;
            s.pos    = p;
            s.result = result;
            samples.push_back(s);
        }
        std::cout << "  game " << (g + 1) << "/" << cfg.games << ": "
                  << (whiteScore == 1 ? "1-0" : (whiteScore == 0 ? "0-1" : "1/2")) << ", "
                  << gamePositions.size() << " positions, " << samples.size() << " total\n"
                  << std::flush;
    }
    out << "  collected " << samples.size() << " positions (" << whiteWins << " white wins, "
        << blackWins << " black wins, " << draws << " draws)\n";
}

}  // namespace

int runTune(const TuneConfig& config) {
    std::cout << "=================================================================\n";
    std::cout << " Virgin Texel tuning\n";
    std::cout << "   self play games : " << config.games << " at depth " << config.depth << "\n";
    std::cout << "   parameters      : " << weight_count() << "\n";
    std::cout << "   output          : " << config.out << "\n";
    std::cout << "=================================================================\n" << std::flush;

    reset_weights();  // neutral baseline: no style, no Elo knowledge scaling

    std::vector<Sample> samples;
    collectSamples(config, samples, std::cout);
    if (samples.empty()) {
        std::cout << "tune: no positions collected\n";
        return 2;
    }

    // Deterministic train / validation split. Without the validation set the
    // coordinate descent happily overfits a small data set (measured: 11 % lower
    // training error, but clearly weaker in real games).
    std::mt19937 rng(20261030u);
    std::shuffle(samples.begin(), samples.end(), rng);
    const size_t testCount = std::min(samples.size() / 4,
                                      samples.size() * size_t(std::max(0, config.testPercent)) / 100);
    std::vector<Sample> test(samples.begin(), samples.begin() + long(testCount));
    samples.erase(samples.begin(), samples.begin() + long(testCount));
    std::cout << " split: " << samples.size() << " train / " << test.size() << " validation\n"
              << std::flush;

    // ---------------------------------------------------------- K (scaling)
    double bestK   = 300.0;
    double bestErr = 1e30;
    for (int k = 80; k <= 800; k += 20) {
        const double e = meanError(samples, double(k));
        if (e < bestErr) {
            bestErr = e;
            bestK   = double(k);
        }
    }
    for (int k = int(bestK) - 20; k <= int(bestK) + 20; ++k) {
        if (k <= 0)
            continue;
        const double e = meanError(samples, double(k));
        if (e < bestErr) {
            bestErr = e;
            bestK   = double(k);
        }
    }
    const double errorStart = bestErr;
    const double testStart  = meanError(test, bestK);
    std::printf(" K = %.0f cp, train error %.6f, validation error %.6f\n", bestK, errorStart,
                testStart);
    std::cout << std::flush;

    // -------------------------------------------------- coordinate descent
    std::vector<int> before(static_cast<size_t>(weight_count()), 0);
    for (int i = 0; i < weight_count(); ++i)
        before[size_t(i)] = weight_value(weight_def(i).name);

    double error = errorStart;
    for (int pass = 0; pass < config.maxPasses; ++pass) {
        const int step = std::max(1, 8 >> pass);  // 8, 4, 2, 1, 1
        int       improved = 0;
        for (int i = 0; i < weight_count(); ++i) {
            const WeightDef& d    = weight_def(i);
            const int        base = weight_value(d.name);
            // Keep every parameter close to its compiled default: unlimited
            // steps made the search push values to absurd corners.
            const int lim  = std::max(8, std::abs(d.def) * config.maxDeltaPercent / 100);
            const int low  = std::max(d.min, d.def - lim);
            const int high = std::min(d.max, d.def + lim);
            for (int sign : {1, -1}) {
                const int cand = base + sign * step;
                if (cand < low || cand > high)
                    continue;
                set_weight_index(i, cand);
                const double e = meanError(samples, bestK);
                if (e < error) {
                    error = e;
                    ++improved;
                    break;  // keep the improvement and move on
                }
                set_weight_index(i, base);
            }
        }
        std::printf(" pass %d (step %d): error %.6f, %d parameters changed\n", pass + 1, step,
                    error, improved);
        std::cout << std::flush;
        if (improved == 0 && step == 1)
            break;
    }

    // ------------------------------------------------------------- report
    std::vector<std::pair<int, int>> deltas;  // index -> difference
    for (int i = 0; i < weight_count(); ++i) {
        const int now = weight_value(weight_def(i).name);
        if (now != before[size_t(i)])
            deltas.emplace_back(i, now - before[size_t(i)]);
    }
    std::sort(deltas.begin(), deltas.end(),
              [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                  return std::abs(a.second) > std::abs(b.second);
              });

    const double testErr = meanError(test, bestK);
    std::printf("\n train      : %.6f -> %.6f\n", errorStart, error);
    std::printf(" validation : %.6f -> %.6f\n", testStart, testErr);
    std::printf(" %d of %d parameters changed\n", int(deltas.size()), weight_count());

    if (testErr >= testStart) {
        std::cout << " REJECTED: the tuning did not improve the validation set, keeping the\n"
                     " compiled defaults (a tuning that only helps the training set is overfitting)\n";
        reset_weights();
        return 4;
    }

    std::cout << " biggest changes:\n";
    for (size_t i = 0; i < deltas.size() && i < 15; ++i) {
        const WeightDef& d = weight_def(deltas[i].first);
        std::printf("   %-34s %6d -> %6d  (%+d)\n", d.name, before[size_t(deltas[i].first)],
                    weight_value(d.name), deltas[i].second);
    }

    std::string err;
    if (!save_weights_file(config.out, &err)) {
        std::cout << "tune: cannot write " << config.out << " (" << err << ")\n";
        return 3;
    }
    std::cout << "\n ACCEPTED, tuned weights written to " << config.out << "\n"
              << " use them with: setoption name WeightsFile value " << config.out << "\n"
              << "                setoption name ReloadWeights\n"
              << " and measure them: virgin --match <engine> 1320 50 6 1000 2800 "
                 "v2800-ausgeglichen reports 8 0 "
              << config.out << "\n"
              << " (the style profiles in src/botdata.cpp are absolute values; a tuned\n"
              << "  baseline has to be folded into them to reach the bots)\n";
    return 0;
}
