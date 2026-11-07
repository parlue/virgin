// Virgin - UCI chess engine
// Precomputed attack tables. Sliding attacks use magic bitboards
// with magics found at startup (deterministic seed, no external files).
#include "bitboard.h"

#include <cstring>

Bitboard PawnAttacks[COLOR_NB][SQUARE_NB];
Bitboard KnightAttacks[SQUARE_NB];
Bitboard KingAttacks[SQUARE_NB];
Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
Bitboard LineBB[SQUARE_NB][SQUARE_NB];

namespace {

struct Magic {
    Bitboard  mask;
    Bitboard  magic;
    Bitboard* attacks;
    unsigned  shift;

    unsigned index(Bitboard occ) const {
        return unsigned(((occ & mask) * magic) >> shift);
    }
};

Magic    RookMagics[SQUARE_NB];
Magic    BishopMagics[SQUARE_NB];
Bitboard RookTable[0x19000];   // 102400
Bitboard BishopTable[0x1480];  // 5248

// Deterministic xorshift64* generator (no <random> to keep the binary small
// and the magic tables reproducible across platforms).
uint64_t rngState = 0x9E3779B97F4A7C15ULL;

uint64_t rnd64() {
    rngState ^= rngState >> 12;
    rngState ^= rngState << 25;
    rngState ^= rngState >> 27;
    return rngState * 0x2545F4914F6CDD1DULL;
}

// Sparse random number: few bits set -> good magic candidates.
uint64_t rnd_sparse() { return rnd64() & rnd64() & rnd64(); }

Bitboard sliding_attack(PieceType pt, Square sq, Bitboard occupied) {
    static const int rookDeltas[4][2]   = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    static const int bishopDeltas[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    const int (*deltas)[2] = (pt == ROOK) ? rookDeltas : bishopDeltas;

    Bitboard attacks = 0;
    const int f0 = file_of(sq);
    const int r0 = rank_of(sq);
    for (int d = 0; d < 4; ++d) {
        int f = f0 + deltas[d][0];
        int r = r0 + deltas[d][1];
        while (f >= 0 && f < 8 && r >= 0 && r < 8) {
            const Square t = make_square(f, r);
            attacks |= square_bb(t);
            if (occupied & square_bb(t))
                break;
            f += deltas[d][0];
            r += deltas[d][1];
        }
    }
    return attacks;
}

void init_magics(PieceType pt, Bitboard table[], Magic magics[]) {
    Bitboard occupancy[4096];
    Bitboard reference[4096];
    int size = 0;

    for (int s = SQ_A1; s <= SQ_H8; ++s) {
        const Square sq = Square(s);
        Magic& m = magics[sq];

        // Relevant occupancy: all blockers except the board edges.
        const Bitboard edges =
            ((Rank1BB | Rank8BB) & ~rank_bb(sq)) | ((FileABB | FileHBB) & ~file_bb(sq));
        m.mask  = sliding_attack(pt, sq, 0) & ~edges;
        m.shift = unsigned(64 - popcount(m.mask));
        m.attacks = (sq == SQ_A1) ? table : magics[sq - 1].attacks + size;

        // Carry-rippler enumeration of all subsets of the mask.
        int count = 0;
        Bitboard b = 0;
        do {
            occupancy[count] = b;
            reference[count] = sliding_attack(pt, sq, b);
            ++count;
            b = (b - m.mask) & m.mask;
        } while (b);
        size = count;

        // Search a magic multiplier that maps every subset uniquely.
        for (int tries = 0; tries < 100000000; ++tries) {
            const Bitboard candidate = rnd_sparse();
            if (popcount((candidate * m.mask) >> 56) < 6)
                continue;

            std::memset(m.attacks, 0, size_t(count) * sizeof(Bitboard));
            bool ok = true;
            for (int i = 0; i < count; ++i) {
                Bitboard* slot = m.attacks + ((occupancy[i] & m.mask) * candidate >> m.shift);
                if (*slot == 0)
                    *slot = reference[i];
                else if (*slot != reference[i]) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                m.magic = candidate;
                break;
            }
        }
    }
}

}  // namespace

Bitboard bishop_attacks(Square s, Bitboard occ) {
    const Magic& m = BishopMagics[s];
    return m.attacks[m.index(occ)];
}

Bitboard rook_attacks(Square s, Bitboard occ) {
    const Magic& m = RookMagics[s];
    return m.attacks[m.index(occ)];
}

void init_bitboards() {
    // Pawn, knight, king attack tables.
    for (int s = 0; s < 64; ++s) {
        const Square sq = Square(s);
        const Bitboard b = square_bb(sq);

        PawnAttacks[WHITE][sq] = shift_ne(b) | shift_nw(b);
        PawnAttacks[BLACK][sq] = shift_se(b) | shift_sw(b);

        Bitboard n = 0;
        n |= (b & ~FileABB) << 15;
        n |= (b & ~FileHBB) << 17;
        n |= (b & ~(FileABB | FileBBB)) << 6;
        n |= (b & ~(FileGBB | FileHBB)) << 10;
        n |= (b & ~FileABB) >> 17;
        n |= (b & ~FileHBB) >> 15;
        n |= (b & ~(FileABB | FileBBB)) >> 10;
        n |= (b & ~(FileGBB | FileHBB)) >> 6;
        KnightAttacks[sq] = n;

        KingAttacks[sq] = shift_north(b) | shift_south(b) | shift_east(b) | shift_west(b)
                        | shift_ne(b) | shift_nw(b) | shift_se(b) | shift_sw(b);
    }

    init_magics(ROOK, RookTable, RookMagics);
    init_magics(BISHOP, BishopTable, BishopMagics);

    // Between / line tables (used later for pinned pieces and check evasion).
    for (int a = 0; a < 64; ++a) {
        const Square s1 = Square(a);
        for (int b = 0; b < 64; ++b) {
            const Square s2 = Square(b);
            BetweenBB[s1][s2] = 0;
            LineBB[s1][s2]    = 0;
            if (s1 == s2)
                continue;

            if (rook_attacks(s1, 0) & square_bb(s2)) {
                LineBB[s1][s2] = (rook_attacks(s1, 0) & rook_attacks(s2, 0))
                               | square_bb(s1) | square_bb(s2);
                BetweenBB[s1][s2] = rook_attacks(s1, square_bb(s2))
                                  & rook_attacks(s2, square_bb(s1));
            } else if (bishop_attacks(s1, 0) & square_bb(s2)) {
                LineBB[s1][s2] = (bishop_attacks(s1, 0) & bishop_attacks(s2, 0))
                               | square_bb(s1) | square_bb(s2);
                BetweenBB[s1][s2] = bishop_attacks(s1, square_bb(s2))
                                  & bishop_attacks(s2, square_bb(s1));
            }

        }
    }
}

