// Virgin - UCI chess engine
// Match runner: plays our engine against an external UCI engine and estimates
// our Elo by increasing the opponent's UCI_Elo level by level.
#include "match.h"

#include "botdata.h"
#include "eval.h"
#include "movegen.h"
#include "position.h"
#include "search.h"
#include "weights.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <signal.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

namespace {

// ---------------------------------------------------------------------------
// Child process speaking UCI over stdin/stdout.
// ---------------------------------------------------------------------------
class ChildProcess {
public:
    ~ChildProcess() { terminate(); }

    bool start(const std::string& exePath) {
#ifdef _WIN32
        SECURITY_ATTRIBUTES sa{};
        sa.nLength        = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE childIn = nullptr, childOut = nullptr;
        if (!CreatePipe(&childIn, &inWrite_, &sa, 0))
            return false;
        if (!CreatePipe(&outRead_, &childOut, &sa, 0)) {
            CloseHandle(childIn);
            CloseHandle(inWrite_);
            return false;
        }
        SetHandleInformation(inWrite_, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(outRead_, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si{};
        si.cb         = sizeof(si);
        si.dwFlags    = STARTF_USESTDHANDLES;
        si.hStdInput  = childIn;
        si.hStdOutput = childOut;
        si.hStdError  = childOut;

        PROCESS_INFORMATION pi{};
        std::string cmdLine = "\"" + exePath + "\"";
        const BOOL ok = CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, TRUE,
                                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        CloseHandle(childIn);
        CloseHandle(childOut);
        if (!ok)
            return false;
        process_ = pi.hProcess;
        CloseHandle(pi.hThread);
#else
        int inPipe[2], outPipe[2];
        if (pipe(inPipe) != 0 || pipe(outPipe) != 0)
            return false;
        const pid_t pid = fork();
        if (pid < 0)
            return false;
        if (pid == 0) {
            dup2(inPipe[0], 0);
            dup2(outPipe[1], 1);
            dup2(outPipe[1], 2);
            close(inPipe[0]);
            close(inPipe[1]);
            close(outPipe[0]);
            close(outPipe[1]);
            execl(exePath.c_str(), exePath.c_str(), static_cast<char*>(nullptr));
            _exit(127);
        }
        close(inPipe[0]);
        close(outPipe[1]);
        pid_     = pid;
        inWrite_ = inPipe[1];
        outRead_ = outPipe[0];
#endif
        running_ = true;
        reader_  = std::thread([this] { readLoop(); });
        return true;
    }

    void sendLine(const std::string& line) {
        const std::string data = line + "\n";
#ifdef _WIN32
        DWORD written = 0;
        WriteFile(inWrite_, data.data(), DWORD(data.size()), &written, nullptr);
#else
        const ssize_t ignored = ::write(inWrite_, data.data(), data.size());
        (void)ignored;
#endif
    }

    bool readLine(std::string& line, int timeoutMs) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                          [this] { return !lines_.empty() || !running_; }))
            return false;
        if (lines_.empty())
            return false;
        line = lines_.front();
        lines_.pop();
        return true;
    }

    void terminate() {
#ifdef _WIN32
        if (!running_ && process_ == nullptr)
            return;
#else
        if (!running_ && pid_ == -1)
            return;
#endif
        running_ = false;
        cv_.notify_all();
#ifdef _WIN32
        if (process_) {
            TerminateProcess(process_, 0);
            CloseHandle(process_);
            process_ = nullptr;
        }
        if (inWrite_) { CloseHandle(inWrite_); inWrite_ = nullptr; }
        if (outRead_) { CloseHandle(outRead_); outRead_ = nullptr; }
#else
        if (pid_ > 0) {
            kill(pid_, SIGKILL);
            int status = 0;
            waitpid(pid_, &status, 0);
            pid_ = -1;
        }
        if (inWrite_ >= 0) { close(inWrite_); inWrite_ = -1; }
        if (outRead_ >= 0) { close(outRead_); outRead_ = -1; }
#endif
        if (reader_.joinable())
            reader_.join();
    }

private:
    void readLoop() {
        std::string pending;
        char        buffer[4096];
        while (running_) {
#ifdef _WIN32
            DWORD available = 0;
            if (!PeekNamedPipe(outRead_, nullptr, 0, nullptr, &available, nullptr))
                break;
            if (available == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            DWORD got = 0;
            if (!ReadFile(outRead_, buffer, sizeof(buffer), &got, nullptr) || got == 0)
                break;
#else
            const ssize_t got = ::read(outRead_, buffer, sizeof(buffer));
            if (got <= 0)
                break;
#endif
            pending.append(buffer, size_t(got));
            size_t pos = 0;
            while ((pos = pending.find('\n')) != std::string::npos) {
                std::string line = pending.substr(0, pos);
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                pending.erase(0, pos + 1);
                std::lock_guard<std::mutex> lock(mutex_);
                lines_.push(std::move(line));
                cv_.notify_all();
            }
        }
        running_ = false;
        cv_.notify_all();
    }

    std::thread             reader_;
    std::atomic<bool>       running_{false};
    std::mutex              mutex_;
    std::condition_variable cv_;
    std::queue<std::string> lines_;
#ifdef _WIN32
    HANDLE process_ = nullptr;
    HANDLE inWrite_ = nullptr;
    HANDLE outRead_ = nullptr;
#else
    pid_t pid_      = -1;
    int   inWrite_  = -1;
    int   outRead_  = -1;
#endif
};

}  // namespace

// ---------------------------------------------------------------------------
// Match logic
// ---------------------------------------------------------------------------
const std::vector<std::vector<const char*>>& openings() {
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

struct GameResult {
    int         score = 0;  // 1 = our win, 0 = draw, -1 = our loss
    std::string reason;
    std::string moves;
    int         plies = 0;
};

// Maps a parsed UCI move (from/to plus promotion letter) onto the matching
// legal move of the position, so the internal flag bits (castling, en passant)
// are set correctly. Returns 0 when the move is not legal.
Move findLegalMove(const MoveList& list, Move parsed) {
    for (int i = 0; i < list.count; ++i) {
        const Move m = list.moves[size_t(i)];
        if (from_sq(m) != from_sq(parsed) || to_sq(m) != to_sq(parsed))
            continue;
        if (is_promotion(m) != is_promotion(parsed))
            continue;
        if (is_promotion(m) && promotion_type(m) != promotion_type(parsed))
            continue;
        return m;
    }
    return 0;
}

int materialBalance(const Position& pos) {
    int balance = 0;
    for (int c = 0; c < COLOR_NB; ++c)
        for (int pt = PAWN; pt <= QUEEN; ++pt)
            balance += (c == WHITE ? 1 : -1) * popcount(pos.pieces(Color(c), PieceType(pt)))
                     * EW.matMg[pt];
    return balance;
}

GameResult playGame(ChildProcess& opponent, const MatchConfig& cfg, Color ourColor,
                    const std::vector<const char*>& opening, int oppMs) {
    GameResult  result;
    Position    pos;
    std::string error;
    pos.setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &error);

    std::vector<std::string> moves;
    std::vector<Key>         keys{pos.key()};

    for (const char* text : opening) {
        const Move m = find_legal_move(pos, text);
        if (m == 0)
            break;
        pos.applyMove(m);
        moves.push_back(text);
        keys.push_back(pos.key());
    }

    for (int ply = 0; ply < cfg.maxMoves; ++ply) {
        MoveList legalMoves;
        if (generateLegal(pos, legalMoves) == 0) {
            if (pos.inCheck()) {
                const bool weAreMated = (pos.sideToMove() == ourColor);
                result.score  = weAreMated ? -1 : 1;
                result.reason = weAreMated ? "checkmate against us" : "checkmate for us";
            } else {
                result.score  = 0;
                result.reason = "stalemate";
            }
            break;
        }
        if (pos.halfmoveClock() >= 100) {
            result.score  = 0;
            result.reason = "50 move rule";
            break;
        }
        int repeats = 0;
        for (Key k : keys)
            if (k == pos.key())
                ++repeats;
        if (repeats >= 3) {
            result.score  = 0;
            result.reason = "threefold repetition";
            break;
        }

        Move move = 0;
        std::cout << "      ply " << (ply + 1) << "  "
                  << (pos.sideToMove() == WHITE ? "white" : "black") << "  "
                  << (pos.sideToMove() == ourColor ? "us " : "sf ") << std::flush;
        if (pos.sideToMove() == ourColor) {
            SearchLimits       limits;
            limits.movetime = cfg.movetimeMs;
            std::ostringstream sink;
            const SearchResult r = searchPosition(pos, limits, keys, sink);
            move                 = r.best;
            std::cout << move_to_uci(move) << " (score " << r.score << ", depth " << r.depth << ")"
                      << std::flush;
        } else {
            std::string position = "position startpos moves";
            for (const std::string& m : moves)
                position += " " + m;
            opponent.sendLine(position);
            opponent.sendLine("go movetime " + std::to_string(oppMs));

            std::string line;
            bool        got      = false;
            const auto  deadline = std::chrono::steady_clock::now()
                                 + std::chrono::milliseconds(oppMs + 15000);
            while (std::chrono::steady_clock::now() < deadline) {
                if (!opponent.readLine(line, 500))
                    continue;
                if (line.rfind("bestmove", 0) == 0) {
                    got = true;
                    break;
                }
            }
            if (!got) {
                result.score  = 1;
                result.reason = "opponent timeout";
                std::cout << "TIMEOUT\n";
                break;
            }
            const std::string tail = position.size() > 40 ? "..." + position.substr(position.size() - 40)
                                                          : position;
            std::cout << "(" << tail << ") -> " << line << std::flush;
            std::istringstream is(line);
            std::string        token, moveText;
            is >> token >> moveText;
            bool ok = false;
            move    = parse_uci_move(moveText, ok);
            if (!ok) {
                result.score  = 1;
                result.reason = "opponent sent no move";
                std::cout << "  -> unparsable\n";
                break;
            }
        }

        if (move == 0) {
            std::cout << "  -> no move\n";
            result.score  = (pos.sideToMove() == ourColor) ? -1 : 1;
            result.reason = "no move available";
            break;
        }
        // The opponent is an external process: map its move onto the legal move
        // list. Position::isLegal() only filters pseudo legal moves by king
        // safety, so it must never be used for untrusted input.
        const Move matched = findLegalMove(legalMoves, move);
        if (matched == 0) {
            std::cout << "  -> ILLEGAL\n";
            result.score = (pos.sideToMove() == ourColor) ? -1 : 1;
            result.reason = (pos.sideToMove() == ourColor) ? "we played an illegal move"
                                                          : "opponent played an illegal move";
            break;
        }
        std::cout << "  legal\n";
        pos.applyMove(matched);
        moves.push_back(move_to_uci(matched));
        keys.push_back(pos.key());
    }

    if (result.reason.empty()) {
        const int balance = materialBalance(pos);
        if (balance > 150)       { result.score = 1;  result.reason = "adjudicated: material up"; }
        else if (balance < -150) { result.score = -1; result.reason = "adjudicated: material down"; }
        else                     { result.score = 0;  result.reason = "adjudicated: equal"; }
    }

    for (size_t i = 0; i < moves.size(); ++i)
        result.moves += (i ? " " : "") + moves[i];
    result.plies = int(moves.size());
    return result;
}

// ---------------------------------------------------------------------------
// Match driver
// ---------------------------------------------------------------------------
bool waitReady(ChildProcess& opponent, int timeoutMs = 30000) {
    std::string line;
    const auto  deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (!opponent.readLine(line, 500))
            continue;
        if (line == "readyok")
            return true;
    }
    return false;
}

std::string sanitizeFileName(const std::string& text) {
    std::string out;
    for (char c : text)
        out += (c == '/' || c == '\\' || c == ':') ? '_' : c;
    return out;
}

int runMatch(const MatchConfig& config) {
    const StyleDef* style = styleFromBotId(config.ourBot);
    if (style == nullptr) {
        std::cout << "match: unknown bot id '" << config.ourBot << "' (see --list-bots)\n";
        return 2;
    }
    const int ourProfileElo = botEloFromId(config.ourBot, ELO_MAX);
    applyStyle(style->id);
    applyElo(ourProfileElo);
    if (!config.weightsFile.empty()) {
        std::string err;
        if (!load_weights_file(config.weightsFile, &err))
            std::cout << "match: cannot load weights file " << config.weightsFile << " (" << err
                      << ")\n";
        else
            std::cout << "   weights : " << config.weightsFile << " (loaded over the style)\n";
    }

    std::cout << "=================================================================\n";
    std::cout << " Virgin match\n";
    std::cout << "   us      : " << config.ourBot << "  (style '" << style->id
              << "', Elo profile " << ourProfileElo << ")\n";
    std::cout << "   opponent: " << config.opponent << "\n";
    std::cout << "   levels  : start " << config.startElo << ", step " << config.step << ", "
              << config.gamesPerLevel << " games/level, " << config.movetimeMs << " ms/move";
    if (config.opponentMovetimeMs > 0)
        std::cout << " (opponent " << config.opponentMovetimeMs << " ms)";
    std::cout << "\n";
    if (config.ascendingOnly)
        std::cout << "   mode    : ascending ladder up to Elo " << config.maxElo << "\n";
    else if (config.opponentMovetimeMs > 0)
        std::cout << "   mode    : single level (opponent time fixed at "
                  << config.opponentMovetimeMs << " ms), at most " << config.maxLevels
                  << " levels\n";
    else
        std::cout << "   mode    : spot search - Elo first, then opponent time down to 30 ms, at most "
                  << config.maxLevels << " levels, hopeless levels aborted early\n";
    std::cout << "=================================================================\n" << std::flush;

    ChildProcess opponent;
    if (!opponent.start(config.opponent)) {
        std::cout << "match: cannot start opponent engine '" << config.opponent << "'\n";
        return 3;
    }
    opponent.sendLine("uci");
    {
        std::string line;
        bool        ok       = false;
        const auto  deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        while (std::chrono::steady_clock::now() < deadline) {
            if (!opponent.readLine(line, 500))
                continue;
            if (line.rfind("id name", 0) == 0)
                std::cout << "   " << line << "\n";
            if (line == "uciok") {
                ok = true;
                break;
            }
        }
        if (!ok) {
            std::cout << "match: opponent did not answer 'uci'\n";
            return 4;
        }
    }
    opponent.sendLine("setoption name Threads value 1");
    opponent.sendLine("setoption name Hash value 64");
    opponent.sendLine("setoption name Move Overhead value 30");
    // Our own side: Lazy SMP thread count (1 unless --match was given one).
    setThreads(config.ourThreads);
    opponent.sendLine("isready");
    if (!waitReady(opponent)) {
        std::cout << "match: opponent did not answer 'isready'\n";
        return 5;
    }

    std::ofstream log(config.logDir + "/match-" + sanitizeFileName(config.ourBot) + "-"
                      + std::to_string(config.startElo) + "-" + std::to_string(config.maxElo) + "-"
                      + std::to_string(config.movetimeMs) + "ms.pgn");
    if (log)
        log << "[Event \"Virgin vs opponent\"]\n[Site \"local\"]\n[Date \"local\"]\n"
            << "[TimeControl \"" << config.movetimeMs << " ms/move\"]\n[OurBot \""
            << config.ourBot << "\"]\n[Opponent \"" << config.opponent << "\"]\n\n";

    int totalWins = 0, totalDraws = 0, totalLosses = 0, totalGames = 0;

    // Wall clock since the match started. Without it a run that is still going
    // looks like a hang: the log shows games, not how much time has passed.
    const auto matchStart  = std::chrono::steady_clock::now();
    const auto elapsedText = [&matchStart]() {
        const auto secs =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()
                                                             - matchStart)
                .count();
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d:%02d", int(secs / 60), int(secs % 60));
        return std::string(buf);
    };

    struct LevelOutcome {
        int    wins = 0, draws = 0, losses = 0;
        double score = 0.0;
    };

    // Plays every game of one level: the opponent gets `elo` and `oppMs` ms per
    // move. A level is abandoned as soon as a 50 % score is out of reach, so a
    // hopeless level costs two games instead of all of them.
    const auto playLevel = [&](int elo, int oppMs) {
        LevelOutcome outcome;
        opponent.sendLine("setoption name UCI_LimitStrength value true");
        opponent.sendLine("setoption name UCI_Elo value " + std::to_string(elo));
        opponent.sendLine("isready");
        waitReady(opponent);

        std::cout << "\n--- level: opponent Elo " << elo << " with " << oppMs
                  << " ms/move (we play " << config.movetimeMs << " ms) ---\n";

        for (int g = 0; g < config.gamesPerLevel; ++g) {
            const Color ourColor = (g % 2 == 0) ? WHITE : BLACK;
            const auto& opening  = openings()[size_t(g / 2) % openings().size()];

            opponent.sendLine("ucinewgame");
            opponent.sendLine("isready");
            waitReady(opponent);
            newSearchGame();

            const GameResult r = playGame(opponent, config, ourColor, opening, oppMs);
            if (r.score > 0) ++outcome.wins;
            else if (r.score < 0) ++outcome.losses;
            else ++outcome.draws;

            std::cout << "  game " << (g + 1) << "/" << config.gamesPerLevel << "  ["
                      << elapsedText() << "]  we play "
                      << (ourColor == WHITE ? "white" : "black") << ", opening " << (g / 2 + 1)
                      << " -> " << (r.score > 0 ? "WIN" : (r.score < 0 ? "LOSS" : "DRAW")) << " ("
                      << r.plies << " plies, " << r.reason << ")\n"
                      << std::flush;

            if (log) {
                const bool weWon = (r.score > 0 && ourColor == WHITE)
                                || (r.score < 0 && ourColor == BLACK);
                log << "[Round \"" << elo << "." << (g + 1) << "\"]\n"
                    << "[White \"" << (ourColor == WHITE ? config.ourBot : "opponent") << "\"]\n"
                    << "[Black \"" << (ourColor == BLACK ? config.ourBot : "opponent") << "\"]\n"
                    << "[Result \""
                    << (r.score == 0 ? "1/2-1/2" : (weWon ? "1-0" : "0-1")) << "\"]\n"
                    << "[Termination \"" << r.reason << "\"]\n\n" << r.moves << "\n\n";
            }

            // Abort the level as soon as 50 % is mathematically out of reach:
            // even winning every remaining game would not get us there.
            const int gamesLeft = config.gamesPerLevel - (g + 1);
            if (2 * outcome.wins + outcome.draws + 2 * gamesLeft < config.gamesPerLevel) {
                std::cout << "  -> 50 % unreachable, level aborted after " << (g + 1) << " games\n"
                          << std::flush;
                break;
            }
        }

        outcome.score = (outcome.wins + 0.5 * outcome.draws) / double(config.gamesPerLevel);
        std::printf("  >>> level %4d: +%d =%d -%d   score %5.1f %%\n", elo, outcome.wins,
                    outcome.draws, outcome.losses, 100.0 * outcome.score);
        std::cout << std::flush;

        totalWins += outcome.wins;
        totalDraws += outcome.draws;
        totalLosses += outcome.losses;
        totalGames += config.gamesPerLevel;
        return outcome;
    };

    // Elo search: start at `startElo` and walk towards the level where the score
    // is 50 %. A level we score above 50 % against is a lower bound for our
    // strength, a level we lose against is an upper bound; the next level is the
    // midpoint of the bracket. The pure upward ladder (ascendingOnly) just walks
    // up in `step` increments like a classic Elo staircase.
    // Spot search. Difficulty has two axes: the opponent's Elo and - once the
    // Elo floor (1320 for Stockfish) is reached - the opponent's thinking time.
    // We walk from the start level towards the point where the score is 50 %:
    //   * win  -> make it harder (more Elo, then more time for the opponent)
    //   * lose -> make it easier (less Elo, then less time for the opponent)
    // The time axis is only used when the caller did not fix the opponent time.
    const bool timeAxis = config.opponentMovetimeMs == 0;
    const int  minOppMs = 30;

    int  elo = config.startElo;
    int  oppMs = timeAxis ? config.movetimeMs : config.opponentMovetimeMs;
    int  loElo = 0, hiElo = 0, loMs = 0, hiMs = 0;
    bool haveLo = false, haveHi = false;
    int  played = 0, lastElo = -1, lastMs = -1;

    while (played < config.maxLevels) {
        const LevelOutcome outcome = playLevel(elo, oppMs);
        ++played;
        lastElo = elo;
        lastMs  = oppMs;

        const bool weWin = outcome.score >= 0.5;
        if (weWin) {
            loElo  = elo;
            loMs   = oppMs;
            haveLo = true;
        } else {
            hiElo  = elo;
            hiMs   = oppMs;
            haveHi = true;
        }

        if (config.ascendingOnly) {
            elo += config.step;
            if (elo > config.maxElo)
                break;
            continue;
        }

        // Bracket closed: same Elo, opponent time within a factor of two.
        if (haveLo && haveHi && loElo == hiElo && hiMs <= loMs * 2)
            break;

        if (weWin) {
            // Harder: more Elo while there is room, otherwise more opponent time.
            if (oppMs >= config.movetimeMs || !timeAxis) {
                if (elo >= config.maxElo)
                    break;  // strongest reference reached
                elo = (elo + config.step > config.maxElo) ? config.maxElo : elo + config.step;
            } else {
                oppMs = (oppMs * 2 > config.movetimeMs) ? config.movetimeMs : oppMs * 2;
            }
        } else {
            // Easier: less Elo down to the opponent's floor, then less time.
            if (elo > 1320) {
                elo = (elo - config.step < 1320) ? 1320 : elo - config.step;
            } else if (timeAxis && oppMs > minOppMs) {
                oppMs = (oppMs / 2 < minOppMs) ? minOppMs : oppMs / 2;
            } else {
                break;  // weaker than the weakest reference we can build
            }
        }

        if (elo == lastElo && oppMs == lastMs)
            break;  // no refinement possible
    }
    const double totalScore = (totalWins + 0.5 * totalDraws) / double(totalGames ? totalGames : 1);
    std::printf("\n=================================================================\n");
    std::printf(" overall: +%d =%d -%d   %d games   score %.1f %%\n", totalWins, totalDraws,
                totalLosses, totalGames, 100.0 * totalScore);
    if (totalGames > 0) {
        // The verdict of an A/B run: an Elo delta against 50 % with a 95 %
        // confidence interval, plus how fast the match actually ran.
        // d(Elo)/d(score) = (400/ln 10) / (p(1-p)), so the interval is
        // 1.96 * that * the standard error of the score - roughly 680/sqrt(n)
        // Elo at 50 %. 100 games resolve about +/-70 Elo, 500 games +/-30.
        const double se       = std::sqrt(totalScore * (1.0 - totalScore) / double(totalGames));
        const double eloDelta = (totalScore > 0.0 && totalScore < 1.0)
                                    ? -400.0 * std::log10(1.0 / totalScore - 1.0)
                                    : 0.0;
        const double eloErr   = (totalScore > 0.0 && totalScore < 1.0)
                                    ? 1.96 * (400.0 / std::log(10.0)) * se
                                          / (totalScore * (1.0 - totalScore))
                                    : 0.0;
        const double minutes =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - matchStart).count() / 60.0;
        std::printf(" verdict: %+.0f Elo vs 50 %% (95 %% confidence +/- %.0f)   in %.1f min"
                    "   (%.1f games/min)\n",
                    eloDelta, eloErr, minutes, minutes > 0.0 ? totalGames / minutes : 0.0);
    }
    if (haveLo && haveHi) {
        if (loElo == hiElo)
            std::printf(" SPOT: 50 %% against SF Elo %d with %d .. %d ms/move (we play %d ms)\n", loElo,
                        loMs, hiMs, config.movetimeMs);
        else
            std::printf(" SPOT: 50 %% lies between SF Elo %d and %d (we play %d ms/move)\n", loElo,
                        hiElo, config.movetimeMs);
    } else if (haveLo) {
        std::printf(" Virgin beat every reference (up to SF Elo %d with %d ms/move)\n", loElo, loMs);
    } else if (haveHi) {
        std::printf(" Virgin is weaker than the weakest reference: SF Elo %d with %d ms/move\n",
                    hiElo, hiMs);
        if (timeAxis && hiMs <= minOppMs)
            std::printf("  (opponent time axis exhausted at %d ms - a weaker opponent engine would be\n"
                        "  needed to measure further down)\n",
                        minOppMs);
    } else {
        std::printf(" no level was played\n");
    }
    std::printf(" (%d levels, step %d, %d games per level, opponent Elo range %d..%d)\n", played,
                config.step, config.gamesPerLevel, config.startElo, config.maxElo);
    std::printf("=================================================================\n");
    std::cout << " match log: " << config.logDir << "/match-" << sanitizeFileName(config.ourBot)
              << "-" << config.startElo << "-" << config.maxElo << "-" << config.movetimeMs
              << "ms.pgn\n";

    opponent.sendLine("quit");
    opponent.terminate();
    if (log)
        log << "; overall +" << totalWins << " =" << totalDraws << " -" << totalLosses
            << ", winnable up to SF Elo " << loElo << " with " << loMs << " ms, lost from SF Elo "
            << hiElo << " with " << hiMs << " ms\n";
    return 0;
}
