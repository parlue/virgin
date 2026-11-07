// Virgin - UCI chess engine
// Entry point.
#include "bitboard.h"
#include "embedded_config.h"
#include "position.h"
#include "search.h"
#include "selftest.h"
#include "tt.h"
#include "uci.h"
#include "weights.h"

#include <iostream>

int main(int argc, char** argv) {
    init_bitboards();
    init_zobrist();
    init_weights();  // fills the registry values, refreshes EW and builds the PSTs
    initSearch();    // reduction tables
#ifdef VIRGIN_EMBEDDED
    TT.resizeKiB(VIRGIN_EMBEDDED_HASH_KIB);
#else
    TT.resize(32);
#endif
    //   // default hash size, the Hash option changes it

    // Engine modes: --gen-data, --selftest, --selfplay, --match
    // (returns -1 if none of them was requested).
    const int modeResult = handleEngineModes(argc, argv);
    if (modeResult >= 0)
        return modeResult;

    uciInit(argc, argv);

    if (uciCommandLine(argc, argv))
        return 0;

    return uciLoop(argc, argv);
}
