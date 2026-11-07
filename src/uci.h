// Virgin - UCI chess engine
// UCI front end.
#pragma once

#include <string>
#include <iosfwd>

// Initialise the UCI layer (data files, options). Call after the engine
// subsystems (bitboards, zobrist, weights) are initialised.
void uciInit(int argc, char** argv);

// Run the UCI command loop. Returns the process exit code.
int uciLoop(int argc, char** argv);

// Handle command line arguments (--perft, --bench, --list-bots, --weights,
// --bot <id>). Returns true if a command line action was executed.
bool uciCommandLine(int argc, char** argv);

// Select a bot profile by id (also usable from the command line).
void uciSetBot(const std::string& id);

// In-process library front end. Exactly one live engine session per process.
int uciRun(std::istream& input, std::ostream& output);
void uciShutdown();
void uciResetSession();
