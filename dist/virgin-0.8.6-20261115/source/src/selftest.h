// Virgin - UCI chess engine
// Built-in self test and in-process self play. Both are pure C++ and replace
// the external helper scripts: the engine can verify itself.
#pragma once

#include <iosfwd>
#include <string>

// Runs the complete self test (perft suite, board sanity, Zobrist consistency,
// evaluation symmetry, weight registry, move encoding, bot catalog).
// Returns 0 when everything passes.
int runSelfTest(const std::string& dataDir, std::ostream& out);

// Plays a game between two bot profiles in process and writes a log.
int runSelfPlay(const std::string& botA, const std::string& botB, int maxMoves, int depth,
                const std::string& logPath, std::ostream& out);

// Handles the engine modes --gen-data, --selftest and --selfplay.
// Returns -1 when no engine mode was requested, otherwise the process exit code.
int handleEngineModes(int argc, char** argv);
