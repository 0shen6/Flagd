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

enum class SolveMode {
    Full,  // every deduction both stages can find
    Fast,  // stop after the cheap local stage if it found anything
};

// Sound by construction: every deduction holds in every assignment consistent
// with the visible information. Two stages:
//  1. Local windows around each number (cheap; enough most of the time).
//  2. Exact enumeration of each frontier component, combined against the
//     floor census. Components too large for the budget are skipped.
// Skipping work only means fewer deductions, never wrong ones.
SolverResult solve(const Board& board, SolveMode mode = SolveMode::Full, long nodeBudget = 100000);

} // namespace flagd
