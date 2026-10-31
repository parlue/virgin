// Virgin - UCI chess engine
// Self test and self play implementation.
#include "selftest.h"
#include "adaptive.h"

#include "bitboard.h"
#include "botdata.h"
#include "eval.h"
#include "match.h"
#include "movegen.h"
#include "position.h"
#include "search.h"
#include "tune.h"
#include "tt.h"
#include "weights.h"

#include <algorithm>
#include <atomic>
#include <thread>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <vector>

namespace {

struct PerftCase {
    const char*        name;
    const char*        fen;
    std::vector<uint64_t> counts;
};

const std::vector<PerftCase>& perftSuite() {
    static const std::vector<PerftCase> suite = {
        {"startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
         {20, 400, 8902, 197281, 4865609}},
        {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
         {48, 2039, 97862, 4085603}},
        {"endgame3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
         {14, 191, 2812, 43238, 674624}},
        {"promotion4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
         {6, 264, 9467, 422333}},
        {"position5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
         {44, 1486, 62379, 2103487}},
        {"position6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
         {46, 2079, 89890, 3894594}},
    };
    return suite;
}

// Mirrors a FEN (swap colours, flip ranks) to test evaluation symmetry.
std::string mirrorFen(const std::string& fen) {
    std::istringstream is(fen);
    std::string placement, side, castle, ep;
    if (!(is >> placement >> side >> castle >> ep))
        return "";

    std::vector<std::string> ranks;
    std::string current;
    for (char c : placement) {
        if (c == '/') {
            ranks.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    ranks.push_back(current);
    std::reverse(ranks.begin(), ranks.end());

    std::string outPlacement;
    for (size_t i = 0; i < ranks.size(); ++i) {
        if (i)
            outPlacement += '/';
        for (char c : ranks[i]) {
            if (c >= 'a' && c <= 'z')
                outPlacement += char(c - 32);
            else if (c >= 'A' && c <= 'Z')
                outPlacement += char(c + 32);
            else
                outPlacement += c;
        }
    }

    const std::string outSide = (side == "w") ? "b" : "w";

    std::string outCastle;
    for (char c : castle) {
        switch (c) {
        case 'K': outCastle += 'k'; break;
        case 'Q': outCastle += 'q'; break;
        case 'k': outCastle += 'K'; break;
        case 'q': outCastle += 'Q'; break;
        case '-': outCastle += '-'; break;
        default: break;
        }
    }
    if (outCastle.empty())
        outCastle = "-";

    std::string outEp = "-";
    if (ep.size() >= 2 && ep != "-")
        outEp = std::string(1, ep[0]) + std::string(1, char('1' + (7 - (ep[1] - '1'))));

    return outPlacement + " " + outSide + " " + outCastle + " " + outEp + " 0 1";
}

int countBotLines(const std::string& path) {
    std::ifstream in(path);
    if (!in)
        return -1;
    int count = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        ++count;
    }
    return count;
}

}  // namespace

int runSelfTest(const std::string& dataDir, std::ostream& out) {
    int failures = 0;
    const auto check = [&](bool ok, const std::string& what) {
        out << (ok ? "ok    " : "FAIL  ") << what << "\n";
        if (!ok)
            ++failures;
    };

    // ------------------------------------------------------- perft suite
    for (const PerftCase& c : perftSuite()) {
        Position pos;
        std::string error;
        if (!pos.setFen(c.fen, &error)) {
            check(false, std::string("perft ") + c.name + ": bad fen (" + error + ")");
            continue;
        }
        for (size_t depth = 1; depth <= c.counts.size(); ++depth) {
            const uint64_t nodes = perft(pos, int(depth));
            const std::string label = std::string("perft ") + c.name + " d" + std::to_string(depth)
                                    + " = " + std::to_string(nodes);
            if (nodes != c.counts[depth - 1]) {
                check(false, label + " (expected " + std::to_string(c.counts[depth - 1]) + ")");
                break;
            }
            check(true, label);
        }
    }

    // ------------------------------------- board sanity over a random walk
    {
        Position pos;
        std::string error;
        pos.setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &error);
        std::mt19937 rng(20260101u);
        int  plies = 0;
        bool ok    = true;

        for (int ply = 0; ply < 200; ++ply) {
            MoveList  list;
            const int legal = generateLegal(pos, list);
            if (legal == 0)
                break;
            if (legal != int(list.size())) {
                ok = false;
                break;
            }
            std::uniform_int_distribution<int> dist(0, legal - 1);
            pos.applyMove(list[dist(rng)]);

            std::string msg;
            if (!pos.sanityCheck(&msg)) {
                ok = false;
                out << "      sanity: " << msg << "\n";
                break;
            }
            ++plies;
        }
        check(ok && plies > 50, "board sanity + Zobrist key over " + std::to_string(plies) + " plies");
    }

    // --------------------------------------------- evaluation symmetry
    {
        bool ok = true;
        for (const PerftCase& c : perftSuite()) {
            Position    original;
            Position    mirrored;
            std::string error;
            const std::string mirroredFen = mirrorFen(c.fen);
            if (!original.setFen(c.fen, &error) || !mirrored.setFen(mirroredFen, &error)) {
                ok = false;
                out << "      mirror fen failed for " << c.name << "\n";
                continue;
            }
            const Score a = evaluate(original);
            const Score b = evaluate(mirrored);
            if (a != b) {
                ok = false;
                out << "      " << c.name << ": eval " << a << " vs mirrored " << b << "\n";
            }
        }
        check(ok, "evaluation is colour symmetric (eval(pos) == eval(mirror(pos)))");
    }

    // ------------------------------------------------- weight registry
    {
        bool unique    = true;
        bool rangeOk   = true;
        bool defaultOk = true;
        for (int i = 0; i < weight_count(); ++i) {
            const WeightDef& d = weight_def(i);
            if (d.def < d.min || d.def > d.max)
                rangeOk = false;
            if (weight_value(d.name) != d.def)
                defaultOk = false;
            for (int j = i + 1; j < weight_count(); ++j)
                if (std::string(d.name) == weight_def(j).name)
                    unique = false;
        }
        check(unique, "weight names are unique (" + std::to_string(weight_count()) + " parameters)");
        check(rangeOk, "every default lies inside min..max");
        check(defaultOk, "weight_value(name) returns the registry default");
    }

    // ------------------------------------------------ move text round trip
    {
        bool ok = true;
        for (int from = 0; from < 64 && ok; ++from) {
            for (int to = 0; to < 64 && ok; ++to) {
                if (from == to)
                    continue;
                const Move normal = make_move(Square(from), Square(to));
                bool       parsed = false;
                const Move again  = parse_uci_move(move_to_uci(normal), parsed);
                if (!parsed || again != normal)
                    ok = false;

                for (int promo = KNIGHT; promo <= QUEEN && ok; ++promo) {
                    const Move promotion = make_move(Square(from), Square(to), MF_PROMOTION,
                                                     PieceType(promo));
                    bool       parsedPromo = false;
                    const Move againPromo = parse_uci_move(move_to_uci(promotion), parsedPromo);
                    if (!parsedPromo || againPromo != promotion
                        || promotion_type(againPromo) != PieceType(promo))
                        ok = false;
                }
            }
        }
        check(ok, "move encoding round trip (all squares and promotions)");
    }

    // ------------------------------- replaying a UCI move list (castling, ep)
    // Castling arrives as "e1g1" and en passant as an ordinary pawn capture.
    // Both must be resolved against the legal move list: the parsed text alone
    // is only a king/pawn step, which leaves the rook behind or the captured
    // pawn on the board - the engine would then desynchronise.
    {
        std::string fenError;

        Position castle;
        castle.setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &fenError);
        const char* castleMoves[] = {"e2e4", "e7e5", "g1f3", "b8c6", "f1b5",
                                     "a7a6", "b5a4", "g8f6", "e1g1"};
        bool castleOk = true;
        for (const char* text : castleMoves) {
            const Move m = find_legal_move(castle, text);
            if (m == 0) {
                castleOk = false;
                break;
            }
            castle.applyMove(m);
        }
        check(castleOk && castle.pieceOn(SQ_F1) == make_piece(WHITE, ROOK)
                  && castle.pieceOn(SQ_G1) == make_piece(WHITE, KING)
                  && castle.pieceOn(SQ_H1) == NO_PIECE,
              "move list replay: e1g1 castles (king g1, rook f1)");

        Position    ep;
        ep.setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &fenError);
        const char* epMoves[] = {"e2e4", "a7a5", "e4e5", "d7d5", "e5d6"};
        bool epOk = true;
        for (const char* text : epMoves) {
            const Move m = find_legal_move(ep, text);
            if (m == 0) {
                epOk = false;
                break;
            }
            ep.applyMove(m);
        }
        check(epOk && ep.pieceOn(SQ_D6) == make_piece(WHITE, PAWN)
                  && ep.pieceOn(SQ_D5) == NO_PIECE,
              "move list replay: e5d6 takes en passant (d5 empty)");
    }

    // -------------------------------------------------------- bot catalog
    {
        const int styleCount = int(styles().size());
        const int levelCount = int(eloLevels().size());
        const int expected   = styleCount * levelCount;
        check(styleCount == 5, "five style profiles (" + std::to_string(styleCount) + ")");
        check(levelCount == 43, "43 Elo levels 700..2800 step 50 (" + std::to_string(levelCount) + ")");
        check(expected == 215, "bot catalog size = styles x levels (" + std::to_string(expected) + ")");

        bool idsOk = true;
        for (int elo : eloLevels()) {
            const EloHandicap h = eloHandicap(elo);
            if (h.maxDepth < 1 || h.nodeLimit < 1 || h.moveError < 0 || h.blunderPct < 0.0
                || h.moveNoise < 0)
                idsOk = false;
            for (const StyleDef& s : styles()) {
                const std::string id = botId(elo, s.id);
                if (styleFromBotId(id) != &s || botEloFromId(id, -1) != elo
                    || botDisplay(elo, s).empty())
                    idsOk = false;
            }
        }
        check(idsOk, "bot ids resolve back to Elo level and style");

        bool handicapOk = true;
        for (size_t i = 1; i < eloLevels().size(); ++i) {
            const EloHandicap low  = eloHandicap(eloLevels()[i - 1]);
            const EloHandicap high = eloHandicap(eloLevels()[i]);
            if (high.maxDepth < low.maxDepth || high.nodeLimit < low.nodeLimit
                || high.moveError > low.moveError || high.blunderPct > low.blunderPct
                || high.moveNoise > low.moveNoise)
                handicapOk = false;
        }
        check(handicapOk, "Elo handicap is monotone (stronger level never gets weaker limits)");

        const int botsInFile = countBotLines(dataDir + "/bots.txt");
        check(botsInFile == expected, "data/bots.txt contains " + std::to_string(botsInFile)
                                      + " bots (expected " + std::to_string(expected) + ")");

        std::ifstream stylesFile(dataDir + "/styles.txt");
        check(stylesFile.good(), "data/styles.txt is readable");
        std::ifstream eloFile(dataDir + "/elo_profile.txt");
        check(eloFile.good(), "data/elo_profile.txt is readable");
        std::ifstream weightsFile(dataDir + "/weights_default.txt");
        check(weightsFile.good(), "data/weights_default.txt is readable");
    }

    // -------------------------------------------------- style application
    {
        bool       ok          = true;
        const int  baseContempt = weight_value("Misc.Contempt");
        if (!applyStyle("stier"))
            ok = false;
        if (weight_value("Aggression.KingAttack") != 220)
            ok = false;
        if (!applyStyle("ausgeglichen"))
            ok = false;
        if (weight_value("Aggression.KingAttack") != 100 || weight_value("Misc.Contempt") != baseContempt)
            ok = false;
        check(ok, "applyStyle sets the style weights and restores the neutral baseline");
        reset_weights();
    }

    // -------------------------------------------------- static exchange eval
    {
        bool       ok = false;
        Position   pos;
        std::string error;

        pos.setFen("3k4/8/8/3p4/4P3/8/8/3K4 w - - 0 1", &error);
        const Move exd5 = parse_uci_move("e4d5", ok);
        check(ok && seeGE(pos, exd5, 100) && !seeGE(pos, exd5, 200),
              "SEE: a free pawn is worth a pawn");

        pos.setFen("3k4/8/2p5/3p4/4P3/8/8/3K4 w - - 0 1", &error);
        check(seeGE(pos, exd5, 0) && !seeGE(pos, exd5, 1),
              "SEE: a defended pawn is an even trade");

        pos.setFen("4k3/8/8/3q4/4P3/8/8/4K3 w - - 0 1", &error);
        check(seeGE(pos, exd5, 800), "SEE: a hanging queen is captured");
    }

    {
        Position p;
        p.setFen("7k/7p/5n2/7Q/8/8/1B6/6KR w - - 0 1");
        const Move mate = find_legal_move(p, "h5h7");
        check(mate && seeGE(p, mate, 100) && !seeGE(p, mate, 101),
              "SEE excludes a pinned knight and an illegal king recapture on h7");
        p.setFen("7k/8/8/3pP3/8/8/8/K7 w - d6 0 1");
        const Move ep = find_legal_move(p, "e5d6");
        check(ep && seeGE(p, ep, 100) && !seeGE(p, ep, 101),
              "SEE values an unrecaptured en passant pawn correctly");
        p.setFen("k4r2/6P1/8/8/8/8/8/K7 w - - 0 1");
        const Move promo = find_legal_move(p, "g7g8q");
        check(promo && seeGE(p, promo, -100) && !seeGE(p, promo, -99),
              "SEE accounts for promotion followed by rook recapture");
    }

    // Regression: strict geometry, real pins, and correct dead-position rules.
    {
        check(BetweenBB[SQ_A1][SQ_A2] == 0
                  && BetweenBB[SQ_A1][SQ_A3] == square_bb(SQ_A2),
              "between squares exclude both endpoints");
        const auto term = [](const char* fen, int index) {
            Position p; p.setFen(fen); return evaluateDetailed(p).term[index];
        };
        const int pinIndex = weight_index("Threats.Pinned");
        const int pinSaved = weight_value("Threats.Pinned");
        set_weight_index(pinIndex, 0);
        const int fakeBefore = term("k7/8/8/8/r7/8/8/B6K w - - 0 1", 7);
        const int trueBefore = term("k7/8/8/8/b7/8/8/R6K w - - 0 1", 7);
        set_weight_index(pinIndex, 100);
        check(term("k7/8/8/8/r7/8/8/B6K w - - 0 1", 7) == fakeBefore,
              "bishop cannot pin along a file");
        check(term("k7/8/8/8/b7/8/8/R6K w - - 0 1", 7) > trueBefore,
              "rook recognises a single blocker pinned to the king");
        set_weight_index(pinIndex, pinSaved);
        Position p;
        p.setFen("4k3/8/8/8/8/8/8/2B1K3 w - - 0 1");
        check(evaluate(p) == 0, "K+B v K is dead");
        p.setFen("4kb2/8/8/8/8/8/8/2B1K3 w - - 0 1");
        check(evaluate(p) == 0, "same coloured bishops without pawns are dead");
        p.setFen("2n1k3/8/8/8/8/8/8/2B1K3 w - - 0 1");
        const int deadIndex = weight_index("Drawish.Insufficient");
        const int deadSaved = weight_value("Drawish.Insufficient");
        const Score mixed = evaluate(p);
        set_weight_index(deadIndex, 100);
        check(evaluate(p) == mixed, "B v N is not automatically a dead position");
        set_weight_index(deadIndex, deadSaved);
    }

    check(adaptiveRegressionTests(out), "adaptive chooser regressions");
    check(searchRegressionTests(out), "search boundary and move ordering regressions");
    {
        std::atomic<bool> coherent{true};
        std::vector<std::thread> workers;
        for (int t = 0; t < 4; ++t) {
            workers.emplace_back([t, &coherent] {
                for (int n = 1; n <= 5000; ++n) {
                    const int id = t * 5000 + n;
                    const Key key = (Key(id) << 32) | 0x1234;
                    TT.store(key, Move(id), id % 1000, -(id % 1000), 8, BOUND_EXACT, 0);
                    TTEntry entry{};
                    if (TT.probe(key, entry)
                        && (entry.move != Move(id) || entry.score != id % 1000
                            || entry.eval != -(id % 1000) || depthOf(entry) != 8))
                        coherent.store(false);
                }
            });
        }
        for (auto& worker : workers)
            worker.join();
        check(coherent.load(), "shared TT snapshots remain coherent under four colliding writers");
        TT.clear();
    }

    // Bucket replacement protects useful main-search entries from leaf traffic.
    {
        TT.clear();
        bool retained = true;
        TTEntry e{};
        for (int i = 1; i <= 4; ++i)
            TT.store(Key(i) << 32, Move(i), i * 10, 0, i * 2, BOUND_EXACT, 0);
        for (int i = 1; i <= 4; ++i)
            retained &= TT.probe(Key(i) << 32, e) && e.move == Move(i);
        check(retained, "TT retains four colliding full keys in one bucket");
        TT.store(Key(1) << 32, Move(9), 900, 0, 0, BOUND_EXACT, 0);
        TT.store(Key(5) << 32, Move(5), 50, 0, 0, BOUND_EXACT, 0);
        retained = true;
        for (int i = 1; i <= 4; ++i)
            retained &= TT.probe(Key(i) << 32, e) && e.move == Move(i);
        check(retained, "leaf entries preserve deeper current-generation results");
        TT.newSearch();
        TT.store(Key(5) << 32, Move(5), 50, 0, 0, BOUND_EXACT, 0);
        check(TT.probe(Key(5) << 32, e) && !TT.probe(Key(1) << 32, e),
              "aged bucket replaces its least valuable entry");
        TT.clear();
        TT.store(123, Move(1), SCORE_MATE - 7, 0, 6, BOUND_EXACT, 4);
        check(TT.probe(123, e) && scoreFromTT(e.score, 2) == SCORE_MATE - 5,
              "TT mate distance adjusts between root plies");
        TT.clear();
    }

    // ------------------------------------------- search: mate and tactics
    {
        bool       ok = false;
        Position   pos;
        std::string error;
        SearchLimits limits;
        limits.depth = 6;
        std::ostringstream sink;

        pos.setFen("6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1", &error);
        const SearchResult mate = searchPosition(pos, limits, {}, sink);
        check(mate.best == parse_uci_move("a1a8", ok) && is_mate_score(mate.score),
              "search finds mate in 1 (Ra8#), score " + std::to_string(mate.score));

        pos.setFen("4k3/8/8/3q4/4P3/8/8/4K3 w - - 0 1", &error);
        const SearchResult tactic = searchPosition(pos, limits, {}, sink);
        check(tactic.best == parse_uci_move("e4d5", ok),
              "search captures the hanging queen (" + move_to_uci(tactic.best) + ")");

        // The table is indexed with a power of two mask, so the number of
        // entries is the largest power of two that fits into the requested size.
        // With a 16 byte entry that is 2^21 at 32 MB - twice as many as with the
        // previous 24 byte layout. The check pins that invariant down.
        const size_t wantEntries = size_t(32) * 1024 * 1024 / sizeof(TTEntry);
        check(TT.sizeMb() > 0 && TT.hashfull() >= 0 && sizeof(TTEntry) == 16
                  && TT.entries() == wantEntries,
              "transposition table is active (" + std::to_string(TT.sizeMb()) + " MB, "
                  + std::to_string(TT.entries()) + " entries of "
                  + std::to_string(sizeof(TTEntry)) + " bytes)");
        newSearchGame();
    }

    // ------------------------------------------- evaluation terms (M5)
    {
        std::string error;
        const auto evalOf = [&error](const char* fen) {
            Position p;
            p.setFen(fen, &error);
            return evaluateDetailed(p);
        };

        const EvalBreakdown knight = evalOf("4k3/8/8/8/8/8/8/N3K3 w - - 0 1");
        // The material values are calibrated from the outside, so the
        // bounds follow the registry instead of fixed
        // numbers - the check only verifies that material is reported.
        const int knightEg = weight_value("Material.Knight.Eg");
        const int knightMg = weight_value("Material.Knight.Mg");
        check(knight.material > knightEg - 25 && knight.material < knightMg + 25,
              "eval material: extra knight = " + std::to_string(knight.material) + " cp");

        const EvalBreakdown doubled   = evalOf("4k3/8/8/8/8/P7/P7/4K3 w - - 0 1");
        const EvalBreakdown connected = evalOf("4k3/8/8/8/8/8/PP6/4K3 w - - 0 1");
        check(doubled.term[4] < connected.term[4],
              "eval pawns: doubled (" + std::to_string(doubled.term[4]) + ") < connected ("
                  + std::to_string(connected.term[4]) + ")");

        // The passed pawn bonus is rank dependent (steep rise from the fifth
        // rank on, see src/eval.cpp), so the check compares a deep passer with
        // a shallow one instead of a fixed number.
        const EvalBreakdown passerNear = evalOf("8/6k1/4P3/8/8/8/5PK1/8 w - - 0 1");  // rank 6
        const EvalBreakdown passerFar  = evalOf("8/6k1/8/8/4P3/8/5PK1/8 w - - 0 1");  // rank 4
        check(passerNear.term[4] > passerFar.term[4] && passerNear.term[4] > 0
                  && passerNear.term[10] > 0,
              "eval passed pawn: rank 6 (" + std::to_string(passerNear.term[4]) + ") > rank 4 ("
                  + std::to_string(passerFar.term[4]) + "), endgame "
                  + std::to_string(passerNear.term[10]));

        const EvalBreakdown uncastled =
            evalOf("r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/2N2N2/PPPP1PPP/R1BQ1RK1 b kq - 0 1");
        check(uncastled.term[3] < 0,
              "eval king safety: uncastled black king = " + std::to_string(uncastled.term[3]));

        const EvalBreakdown cornerKnight = evalOf("4k3/8/8/8/8/8/8/N3K3 w - - 0 1");
        const EvalBreakdown activeKnight = evalOf("4k3/8/8/8/8/8/8/4K1N1 w - - 0 1");
        check(cornerKnight.term[2] < activeKnight.term[2],
              "eval mobility: corner knight (" + std::to_string(cornerKnight.term[2])
                  + ") < active knight (" + std::to_string(activeKnight.term[2]) + ")");

        const EvalBreakdown deadDraw = evalOf("8/8/8/4k3/8/8/8/4K3 w - - 0 1");
        check(deadDraw.total == 0, "eval drawish: KvK = " + std::to_string(deadDraw.total));

        // Opposite coloured bishops only: the drawish scale must pull the
        // score towards zero. The absolute value is not fixed - it depends on
        // the (tuned) piece square tables - so the check compares the scaled
        // score against the very same position with the scaling switched off.
        // (The bishops must really be on different colours: g1 and c3 are both
        // dark, which is why the old check never reached the term at all.)
        const char* const oppositeFen = "2b1k3/7p/8/8/8/8/P7/2B1K3 w - - 0 1";
        const EvalBreakdown opposite = evalOf(oppositeFen);

        const int drawishIndex = weight_index("Drawish.OppositeBishops");
        const int drawishSaved = weight_value("Drawish.OppositeBishops");
        set_weight_index(drawishIndex, 100);
        const EvalBreakdown unscaled = evalOf(oppositeFen);
        set_weight_index(drawishIndex, drawishSaved);

        check(std::abs(opposite.total) < std::abs(unscaled.total),
              "eval drawish: opposite bishops scaled " + std::to_string(opposite.total)
                  + " < unscaled " + std::to_string(unscaled.total));
    }


    check(evaluationRegressionTests(out), "usable defence and absolute pin regressions");

    // Direct tactical generation must exactly match filtering the full list,
    // including en passant and every promotion, over varied legal positions.
    {
        bool ok = true;
        std::mt19937 rng(30102026u);
        for (const PerftCase& c : perftSuite()) {
            Position p; p.setFen(c.fen);
            for (int ply = 0; ply < 100; ++ply) {
                MoveList all, tactical, legal;
                generatePseudoLegal(p, all);
                generatePseudoLegal(p, tactical, true);
                std::vector<Move> expected;
                for (Move m : all)
                    if (p.isCapture(m) || is_promotion(m))
                        expected.push_back(m);
                std::sort(expected.begin(), expected.end());
                std::sort(tactical.begin(), tactical.end());
                ok &= expected.size() == tactical.size()
                      && std::equal(expected.begin(), expected.end(), tactical.begin());
                generateLegal(p, legal);
                ok &= hasLegalMove(p) == (legal.count != 0);
                if (!legal.count)
                    break;
                p.applyMove(legal[int(rng() % unsigned(legal.count))]);
            }
        }
        check(ok, "direct tactical generator and fast legal witness match complete generation");
    }

    // Replay a full reference game, rather than baking its preferred
    // root moves into a brittle tactical test.
    {
        reset_weights();
        applyStyle("angriffslustig");
        const char* moves =
            "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5c6 d7c6 h2h3 c8e6 e1g1 d8d6 "
            "b2b3 e8c8 c1b2 f7f6 d1e2 c6c5 a2a4 g8e7 b1a3 e7g6 a3c4 d6d7 "
            "e2e3 d7e8 d2d3 g6f4 h3h4 e8h5 g2g3 e6h3 g1h2 h3f1 a1f1 f4e6 "
            "e3e2 g7g5 e2d1 h8g8 c4e3 f8d6 c2c3 d6f8 g3g4 h5f7 h4g5 h7h5 "
            "g5f6 f7f6 e3d5 d8d5 e4d5 h5g4 f3g5 g8h8 h2g2 e6f4 g2g3 f4e2 "
            "g3g2 e2f4 g2g3 f4e2 g3g2";
        Position p; p.setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        std::istringstream input(moves);
        std::string text;
        bool ok = true;
        int ply = 0;
        Score safetyBeforeG3 = 0, safetyAfterG3 = 0;
        while (input >> text) {
            if (ply == 30)
                safetyBeforeG3 = evaluateDetailed(p).term[3];
            const Move m = find_legal_move(p, text);
            if (!m) { ok = false; break; }
            p.applyMove(m); ++ply;
            ok &= p.sanityCheck();
            if (ply == 31)
                safetyAfterG3 = -evaluateDetailed(p).term[3];
        }
        check(ok && ply == 65, "all 65 plies of the reference game replay with consistent board and EP state");
        check(safetyAfterG3 < safetyBeforeG3,
              "16.g3 weakens king safety in the supplied position");
        check(weight_value("Group.Material") == 100 && weight_value("Group.KingSafety") >= 100,
              "competitive attack profile retains full material and king safety knowledge");
        reset_weights();
    }

    // The second user game reaches only two occurrences in the supplied PGN.
    {
        const char* moves = "e2e4 e7e5 g1f3 b8c6 f1c4 f8c5 c2c3 g8f6 d2d4 e5d4 e1g1 d4c3 b1c3 d7d6 c1g5 e8g8 c3d5 c8g4 g5f6 g7f6 d1d3 g8h8 a1e1 c6e5 f3e5 f6e5 d5c3 d8h4 c3d1 f7f5 d1e3 f5f4 e3f5 h4g5 b2b4 c5b4 e1b1 a7a5 a2a3 f8f5 h2h4 g5g6 a3b4 f5h5 b4a5 h5h4 f2f3 g4h3 d3d2 h3c8 a5a6 b7b6 d2d5 a8a7 f1c1 g6g7 d5c6 c8d7 c6d5 d7c8 d5c6 c8d7 c6d5";
        Position p; p.setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        std::vector<Key> history{p.key()};
        std::istringstream input(moves);
        std::string text;
        bool ok = true;
        while (input >> text) {
            const Move m = find_legal_move(p, text);
            if (!m) { ok = false; break; }
            p.applyMove(m);
            ok &= p.sanityCheck();
            history.push_back(p.key());
        }
        check(ok && history.size() == 64, "all 63 plies of the second user game replay legally");
        check(std::count(history.begin(), history.end(), p.key()) == 2,
              "second PGN final position has two occurrences, not three");
        check(popcount(p.pieces(WHITE, ROOK)) == 2 && popcount(p.pieces(BLACK, ROOK)) == 2
                  && popcount(p.pieces(BLACK, PAWN)) - popcount(p.pieces(WHITE, PAWN)) == 2,
              "second PGN retains both rook pairs; White is down two pawns");
    }

    {
        Position p;
        p.setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        std::istringstream input("e2e4 e7e5 g1f3 b8c6 f1b5 g8f6 e1g1 f6e4 d2d4 e4d6 b5c6 d7c6 d4e5 d6f5 d1e2 f5d4 f3d4 d8d4 h2h3 c8f5 f1d1 d4e4 c1e3 e4e5 g2g4 f5d7 b1c3 h7h5 f2f4 e5e7 g4g5 e8c8 e2f3 e7e6 d1d3 e6h3 f3h3 d7h3 d3d8 c8d8 e3d4 h3f5 a1d1 d8c8 d1e1 c6c5 d4e5 f5c2 c3d5 c7c6 d5e3 c2d3 g1g2 h5h4 g2h3 b7b5 b2b3 c5c4 b3c4 b5c4 e1c1 f8a3 c1c4 d3c4 e3c4 a3f8 e5c3 c8d7 c4e5 d7e6 e5c6 e6f5 c3e5 f8c5 e5g7 h8c8 c6d4 f5f4 h3h4 c5d4 g7d4 c8c2 h4h3 c2a2 d4g7 f4g5 g7e5 a7a5 h3g3 g5f5 e5d4 a5a4 g3f3 a2d2 f3e3 d2d4 e3d4 a4a3 d4c3 f5e5 c3b3 f7f5 b3a3 f5f4 a3b3 f4f3 b3c4 f3f2 c4d3 f2f1q d3c3 e5e4 c3b2 e4d3 b2b3 f1b1 b3a3 d3c4 a3a4 b1a2");
        std::string text;
        int count = 0;
        bool legal = true;
        while (input >> text) {
            Move m = find_legal_move(p, text);
            if (!m) { legal = false; break; }
            p.applyMove(m);
            ++count;
            legal &= p.sanityCheck();
        }
        check(legal && count == 120 && p.inCheck() && !hasLegalMove(p),
              "Android loss 1 replays legally through its final checkmate");
    }

    {
        Position p;
        p.setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        std::istringstream input("e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 f8d6 e1g1 b7b5 a4b3 g8f6 d2d4 d8e7 d4e5 d6e5 f3e5 c6e5 b1c3 c8b7 c1g5 e8g8 g1h1 b5b4 c3e2 b7e4 e2g3 d7d5 f2f3 e4g6 f3f4 e5d7 f4f5 c7c6 d1d2 f8e8 a2a3 b4a3 a1a3 f6e4 g3e4 e7e4 f5g6 e4g6 g5e3 d7f6 b3a4 f6e4 d2d4 e8d8 d4b6 d8d6 b6c7 a8f8 e3d4 g6g4 f1g1 g4e2 h2h3 d6g6 a3d3 a6a5 h1h2 h7h6 c7a5 f8c8 a5a6 c8f8 a6a7 f8e8 a7b7 e8e6 b2b4 g6g5 b7b8 g8h7 b8f4 e6g6 f4f3 e4g3 d4e3 g5f5 f3e2 g3e2 g1e1 e2f4 d3d2 g6e6 d2f2 f5f6 e3d4 e6e1 d4f6 e1e4 f6c3 c6c5 a4d7 e4c4 b4b5 c4c3 f2f4 h7g8 b5b6 c3c2 f4b4 c5b4 b6b7 c2f2 b7b8q g8h7 b8b4 f2f6 b4e7 h6h5 d7b5 d5d4 b5d3 h7h6 h3h4 f6f5 d3f5 g7g6 e7f8 h6h7 f8f7 h7h8 f5g6 d4d3 f7f8");
        std::string text;
        int count = 0;
        bool legal = true;
        while (input >> text) {
            Move m = find_legal_move(p, text);
            if (!m) { legal = false; break; }
            p.applyMove(m);
            ++count;
            legal &= p.sanityCheck();
        }
        check(legal && count == 129 && p.inCheck() && !hasLegalMove(p),
              "Android loss 2 replays legally through its final checkmate");
    }

    out << "\n";
    if (failures == 0) {
        out << "RESULT: PASSED\n";
        return 0;
    }
    out << "RESULT: FAILED (" << failures << " problem(s))\n";
    return 1;
}

int runSelfPlay(const std::string& botA, const std::string& botB, int maxMoves, int depth,
                const std::string& logPath, std::ostream& out) {
    std::vector<std::string> log;
    const auto emit = [&](const std::string& text) {
        out << text << "\n";
        log.push_back(text);
    };

    const StyleDef* styleA = styleFromBotId(botA);
    const StyleDef* styleB = styleFromBotId(botB);
    if (!styleA || !styleB) {
        emit("unknown bot id: " + botA + " / " + botB);
        return 2;
    }

    Position    pos;
    std::string error;
    pos.setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &error);

    emit("mode       : built-in self play (in process)");
    emit("Bot weiss  : " + botA + "  (" + styleA->name + ", level "
         + std::to_string(styleA->level) + ")");
    emit("Bot schwarz: " + botB + "  (" + styleB->name + ", level "
         + std::to_string(styleB->level) + ")");
    emit("Modus      : depth " + std::to_string(depth));
    emit("");

    std::vector<std::string> moves;
    int         ply       = 0;
    std::string status    = "move limit reached";
    Score       lastScore = 0;

    for (; ply < maxMoves; ++ply) {
        const bool        whiteToMove = (pos.sideToMove() == WHITE);
        const std::string bot         = whiteToMove ? botA : botB;
        const StyleDef*   style       = whiteToMove ? styleA : styleB;

        applyStyle(style->id);
        applyElo(botEloFromId(bot, ELO_MAX));  // depth, nodes, noise, blunder, knowledge

        SearchLimits       limits;
        limits.depth = depth;
        std::ostringstream sink;
        const SearchResult result = searchPosition(pos, limits, {}, sink);

        if (result.best == 0) {
            status = "mate or stalemate";
            emit("game over after " + std::to_string(ply) + " plies (" + status + ")");
            break;
        }

        lastScore = result.score;
        moves.push_back(move_to_uci(result.best));

        std::ostringstream line;
        line << (ply / 2 + 1) << ". " << (whiteToMove ? "w" : "b") << " "
             << move_to_uci(result.best) << "   score " << result.score << " cp   depth "
             << result.depth;
        emit(line.str());

        pos.applyMove(result.best);
    }

    std::string joined;
    for (size_t i = 0; i < moves.size(); ++i)
        joined += (i ? " " : "") + moves[i];

    emit("");
    emit("moves (UCI): " + joined);
    emit("status: " + status);
    emit("last score: " + std::to_string(lastScore) + " cp");

    if (!logPath.empty()) {
        std::ofstream file(logPath);
        if (file) {
            for (const std::string& line : log)
                file << line << "\n";
            out << "\nlog: " << logPath << "\n";
        }
    }

    reset_weights();
    return 0;
}

int handleEngineModes(int argc, char** argv) {
    if (argc < 2 || !argv || !argv[1])
        return -1;

    const std::string arg = argv[1];

    const auto resolveDataDir = [&](const std::string& explicitDir) {
        if (!explicitDir.empty())
            return explicitDir;
        std::ifstream local("data/bots.txt");
        if (local.good())
            return std::string("data");
        const std::string exe   = argv[0] ? argv[0] : "";
        const size_t      slash = exe.find_last_of("/\\");
        if (slash != std::string::npos)
            return exe.substr(0, slash) + "/data";
        return std::string("data");
    };

    if (arg == "--adaptive-probe") return adaptiveProbe(std::cin, std::cout);

    if (arg == "--gen-data") {
        const std::string dir = argc > 2 ? argv[2] : "data";
        std::string       error;
        const int         written = generateDataFiles(dir, &error);
        if (written < 0) {
            std::cout << "gen-data failed: " << error << std::endl;
            return 1;
        }
        std::cout << "wrote " << written << " data files to " << dir << std::endl;
        return 0;
    }

    if (arg == "--selftest") {
        const std::string dir = resolveDataDir(argc > 2 ? argv[2] : "");
        std::cout << "data dir: " << dir << "\n\n";
        return runSelfTest(dir, std::cout);
    }

    if (arg == "--selfplay") {
        const std::string botA    = argc > 2 ? argv[2] : "v1200-furchtsam";
        const std::string botB    = argc > 3 ? argv[3] : "v1200-stier";
        const int         moves   = argc > 4 ? std::atoi(argv[4]) : 60;
        const int         depth   = argc > 5 ? std::atoi(argv[5]) : 5;
        const std::string logPath = argc > 6 ? argv[6] : "reports/selfplay.txt";
        return runSelfPlay(botA, botB, moves, depth, logPath, std::cout);
    }

    if (arg == "--match") {
        if (argc < 3) {
            std::cout << "usage: virgin --match <opponent.exe> [startElo] [step] [games/level]"
                         " [ms/move] [maxElo] [ourBot] [logDir] [levels|up] [oppMs] [weightsFile]"
                         " [ourThreads]\n"
                      << "example: virgin --match C:/engines/stockfish18.exe 2300 50 4 1000 2800"
                         " v2800-ausgeglichen reports\n"
                      << "  the tool starts at startElo and walks towards the level where the\n"
                      << "  score is 50 %% (down as well as up). 'up' forces a pure ascending\n"
                      << "  ladder, a number sets the maximum number of levels (default 12).\n"
                      << "  oppMs limits the opponent's time per move (Stockfish cannot be\n"
                      << "  limited below Elo 1320, less time is the only way further down).\n";
            return 1;
        }
        MatchConfig config;
        config.opponent      = argv[2];
        config.startElo      = argc > 3 ? std::atoi(argv[3]) : 2300;
        config.step          = argc > 4 ? std::atoi(argv[4]) : 50;
        config.gamesPerLevel = argc > 5 ? std::atoi(argv[5]) : 4;
        config.movetimeMs    = argc > 6 ? std::atoi(argv[6]) : 1000;
        config.maxElo        = argc > 7 ? std::atoi(argv[7]) : 2800;
        config.ourBot        = argc > 8 ? argv[8] : "v2800-ausgeglichen";
        config.logDir        = argc > 9 ? argv[9] : "reports";
        if (argc > 10) {
            const std::string extra = argv[10];
            if (extra == "up")
                config.ascendingOnly = true;
            else
                config.maxLevels = std::max(1, std::atoi(extra.c_str()));
        }
        if (argc > 11) {
            const int oppMs = std::atoi(argv[11]);
            if (oppMs > 0)
                config.opponentMovetimeMs = oppMs;  // 0/empty = walk the time axis
        }
        if (argc > 12)
            config.weightsFile = argv[12];
        if (argc > 13)
            config.ourThreads = std::max(0, std::atoi(argv[13]));
        return runMatch(config);
    }

    if (arg == "--tune") {
        TuneConfig config;
        config.games      = argc > 2 ? std::atoi(argv[2]) : 60;
        config.depth      = argc > 3 ? std::atoi(argv[3]) : 6;
        config.maxPasses  = argc > 4 ? std::atoi(argv[4]) : 5;
        config.out        = argc > 5 ? argv[5] : "data/weights_tuned.txt";
        config.maxDeltaPercent = argc > 6 ? std::atoi(argv[6]) : 20;
        return runTune(config);
    }

    return -1;
}

