#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

// Standalone prototype. Not integrated into Virgin's engine or UCI loop.
namespace virgin_performance {
struct Config {
    double start = 2900, sensitivity = 40, curvature = 1, minimum = 400;
    double ceiling = 2800, endgameDiscount = .30, fullPenaltyLoss = 15;
    int endgamePieceLimit = 4;
    double lossEnter = -150, lossExit = -50, recoveryPoints = 40;
    double recoveryMinCp = 25, recoveryMinPp = 1;
};
inline double winPercent(double cp) {
    if (!std::isfinite(cp)) return 50;
    return 100 / (1 + std::exp(-.00368208 * std::clamp(cp, -1000.0, 1000.0)));
}
inline double accuracy(double loss) {
    if (!std::isfinite(loss)) return 100;
    loss = std::clamp(loss, 0.0, 100.0);
    return std::clamp(103.1668100711649 * std::exp(-.04354415386753951 * loss)
                      - 3.166924740191411 + 1, 0.0, 100.0);
}
inline double penalty(double best, double played, int queens, int pieces, Config c = {}) {
    const double loss = std::max(0.0, winPercent(best) - winPercent(played));
    double factor = 1;
    if (!queens && pieces <= c.endgamePieceLimit)
        factor -= c.endgameDiscount * (1 - std::min(1.0, loss / c.fullPenaltyLoss));
    return (100 - accuracy(loss)) * factor;
}
struct Snapshot { double raw, averagePenalty; int rounded, count; bool above, lossGuard; double windowRaw, recoveryCeiling; };
class Performance {
    Config c;
    std::array<double, 3> last{};
    int count = 0, next = 0;
    double current, anchorRaw, recoveryCeiling;
    bool lossGuard = false;
    std::optional<double> anchorCp;
    std::optional<double> lastKnownCp;
public:
    explicit Performance(Config config = {}) : c(config), current(c.start), anchorRaw(c.start), recoveryCeiling(c.start) {}
    void reset() {
        last.fill(0); count = next = 0; current = anchorRaw = recoveryCeiling = c.start;
        lossGuard = false; anchorCp.reset(); lastKnownCp.reset();
    }
    Snapshot snapshot() const {
        double sum = 0;
        for (int i = 0; i < count; ++i) sum += last[i];
        const double avg = count ? sum / count : 0;
        const double candidate = std::max(c.minimum, c.start - c.sensitivity * std::pow(avg, c.curvature));
        const double raw = current;
        const int rounded = std::min(int(c.ceiling), int(100 * std::floor((raw + 50) / 100)));
        return {raw, avg, rounded, count, raw > c.ceiling, lossGuard, candidate, recoveryCeiling};
    }
    Snapshot add(double value, std::optional<double> position = std::nullopt) {
        if (!std::isfinite(value) || value < 0 || value > 100)
            return snapshot();
        if (position && !std::isfinite(*position)) return snapshot();
        last[next] = value; next = (next + 1) % 3; count = std::min(3, count + 1);
        const double candidate = snapshot().windowRaw;
        if (position) {
            if (*position <= c.lossEnter && !lossGuard) {
                lossGuard = true; anchorCp = *position; anchorRaw = std::min(current, candidate);
            } else if (*position >= c.lossExit) { lossGuard = false; anchorCp.reset(); }
        }
        if (lossGuard) {
            if (candidate < current) {
                current = anchorRaw = candidate;
                if (position) anchorCp = *position;
            }
            recoveryCeiling = anchorRaw;
            if (position && anchorCp) {
                const double gain = winPercent(*position) - winPercent(*anchorCp);
                if (*position - *anchorCp >= c.recoveryMinCp && gain >= c.recoveryMinPp)
                    recoveryCeiling += gain * c.recoveryPoints;
                if (lastKnownCp && *position < *lastKnownCp)
                    recoveryCeiling = std::min(recoveryCeiling, current);
            }
            current = std::min(candidate, std::max(current, recoveryCeiling));
        } else if (!position) { current = std::min(current, candidate); recoveryCeiling = current; }
        else { current = candidate; recoveryCeiling = c.start; }
        if (position) lastKnownCp = *position;
        return snapshot();
    }
};
} // namespace virgin_performance
