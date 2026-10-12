#pragma once
// Deduction engine over threat sums. It sees only what the player sees:
// revealed numbers, visible monsters, and the floor census.
#include <vector>

#include "core/board.h"

namespace flagd {

struct SolverResult {
    std::vector<Pos> safe;          // proven to hold no living monster
    std::vector<Pos> monsters;      // proven to hold a living monster
    std::vector<int> monsterPower;  // parallel to monsters: power if unique, else 0
};

// Sound by construction: every deduction holds in every assignment consistent
// with the visible information. Components too large for the node budget are
// skipped (fewer deductions, never wrong ones).
SolverResult solve(const Board& board, long nodeBudget = 300000);

} // namespace flagd
