#pragma once
// The board: cells, monsters, threat numbers, and safe flood-fill reveals.
// Holds no player state; game_state.cpp applies damage, gold, and so on.
#include <array>
#include <cstdint>
#include <vector>

#include "core/content/content.h"
#include "core/grid.h"

namespace flagd {

enum class Feature : uint8_t { None, Exit, Chest, Key };

// Player annotations on hidden cells. P* marks carry a guessed monster power,
// which lets chords work with threat sums.
enum class Mark : uint8_t { None, Flag, P1, P2, P3, P5, Count };
int markPower(Mark m);  // 0 for None and Flag
Mark nextMark(Mark m);

struct Cell {
    int16_t monster = -1;  // index into Board::monsters(), -1 = none
    Feature feature = Feature::None;
    uint8_t gold = 0;      // picked up when revealed
    Mark mark = Mark::None;
    bool revealed = false;
    bool buried = false;   // re-covered by the Burrower; known safe, dig to reopen
    bool hint = false;     // revealed for free by the generator to avoid a forced guess
    bool chestOpen = false;
};

struct Monster {
    MonsterKind kind{};
    int power = 0;
    int hp = 1;
    bool alive = true;
    bool visible = false;
    std::vector<Pos> cells;
};

class Board {
public:
    Board() = default;
    Board(int w, int h) : cells_(w, h) {}

    int width() const { return cells_.width(); }
    int height() const { return cells_.height(); }
    bool inBounds(Pos p) const { return cells_.inBounds(p); }
    Cell& at(Pos p) { return cells_[p]; }
    const Cell& at(Pos p) const { return cells_[p]; }
    const Grid<Cell>& grid() const { return cells_; }

    std::vector<Monster>& monsters() { return monsters_; }
    const std::vector<Monster>& monsters() const { return monsters_; }

    int addMonster(MonsterKind kind, const std::vector<Pos>& cells);
    Monster* livingMonsterAt(Pos p);
    const Monster* livingMonsterAt(Pos p) const;

    // Sum of power of distinct living monsters touching the 8 neighbours of p
    // (a cell never counts the monster standing on it).
    int threatAt(Pos p) const;

    // True if the player has no information about this cell's contents.
    bool isHidden(Pos p) const;

    // Reveals p (which must hold no living monster), flood-filling through
    // zero-threat cells. Returns every cell newly revealed, including p.
    std::vector<Pos> revealSafe(Pos p);

    // Reveals the cells of a dead monster and floods from any zero-threat
    // cells next to hidden ones. Returns newly revealed cells.
    std::vector<Pos> settleAfterKill(int monsterId);

    bool allSafeRevealed() const;

    // Living monsters per kind; hiddenOnly skips the ones already visible.
    std::array<int, kMonsterKinds> livingCounts(bool hiddenOnly) const;

private:
    void flood(std::vector<Pos> seeds, std::vector<Pos>& out);

    Grid<Cell> cells_;
    std::vector<Monster> monsters_;
};

} // namespace flagd
