#pragma once
#include <cstdint>
#include <vector>

#include "core/board.h"
#include "core/content/balance.h"
#include "core/rng.h"

namespace flagd {

// Places monsters, features, and gold. Cells within safeRadius of firstClick
// stay empty so the first click always opens an area.
Board placeBoard(const balance::FloorParams& params, Pos firstClick, int safeRadius, Rng& rng);

// Plays the board with deductions only, starting at `start`. Whenever no
// deduction exists, a safe cell is revealed for free; those cells are returned.
// An empty result means the board can be fully cleared without guessing.
std::vector<Pos> simulateNoGuess(Board board, Pos start, Rng& rng);

struct GeneratedBoard {
    Board board;
    std::vector<Pos> hints;  // reveal these at the start to remove forced guesses
    int attempts = 0;
};

// Tries several layouts and keeps the one needing the fewest hints (ideally 0).
GeneratedBoard generateBoard(const balance::FloorParams& params, Pos firstClick, int safeRadius,
                             uint64_t seed);

} // namespace flagd
