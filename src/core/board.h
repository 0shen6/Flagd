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

// How cells connect. Hex uses "odd-r" offset rows: odd rows sit half a tile right.
enum class Topology : uint8_t { Square, Wrap, Hex };

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
    bool liar = false;     // shows threat + liarOffset; the player can see it is a liar
    int8_t liarOffset = 0;
    bool clearFog = false; // exact number even on a fogged board
};

struct Monster {
    MonsterKind kind{};
    int power = 0;
    int hp = 1;
    bool alive = true;
    bool visible = false;
    std::vector<Pos> cells;
};

// What a revealed number tells the player: the true threat lies in [lo, hi].
// Liar tiles say nothing reliable (shown = what is drawn).
struct NumberView {
    int lo = 0;
    int hi = 0;
    int shown = 0;
    bool liar = false;
    bool exact() const { return !liar && lo == hi; }
};

class Board {
public:
    Board() = default;
    Board(int w, int h, Topology topo = Topology::Square) : cells_(w, h), topo_(topo) {}

    int width() const { return cells_.width(); }
    int height() const { return cells_.height(); }
    Topology topology() const { return topo_; }
    bool inBounds(Pos p) const { return cells_.inBounds(p); }
    Cell& at(Pos p) { return cells_[p]; }
    const Cell& at(Pos p) const { return cells_[p]; }
    const Grid<Cell>& grid() const { return cells_; }

    bool fogged = false;  // numbers shown as ranges unless clearFog

    std::vector<Monster>& monsters() { return monsters_; }
    const std::vector<Monster>& monsters() const { return monsters_; }

    int addMonster(MonsterKind kind, const std::vector<Pos>& cells);
    Monster* livingMonsterAt(Pos p);
    const Monster* livingMonsterAt(Pos p) const;

    // Neighbours of p under this board's topology (up to 8). Returns the count.
    int neighbors(Pos p, Pos out[8]) const;
    template <class F>
    void forNeighbors(Pos p, F&& f) const {
        Pos n[8];
        const int k = neighbors(p, n);
        for (int i = 0; i < k; ++i) f(n[i]);
    }
    // Graph distance from p to every cell (index by grid().index()); -1 = unreachable.
    std::vector<int> distancesFrom(Pos p) const;
    // Cells within `radius` steps of p, including p itself.
    std::vector<Pos> within(Pos p, int radius) const;

    // Sum of power of distinct living monsters on the neighbours of p
    // (a cell never counts the monster standing on it).
    int threatAt(Pos p) const;
    NumberView view(Pos p) const;

    // True if the player has no information about this cell's contents.
    bool isHidden(Pos p) const;
    // True if every neighbour of p is known (nothing hidden or buried).
    bool surrounded(Pos p) const;

    // Reveals p (which must hold no living monster), flood-filling through
    // zero-threat cells. Returns every cell newly revealed, including p.
    std::vector<Pos> revealSafe(Pos p);

    // Reveals the cells of a dead monster and floods from any zero-threat
    // cells next to hidden ones. Returns newly revealed cells.
    std::vector<Pos> settleAfterKill(int monsterId);

    // Moves a single-cell monster to `to`; `from` becomes empty (not revealed).
    void moveMonster(int monsterId, Pos to);

    bool allSafeRevealed() const;

    // Living monsters per kind; hiddenOnly skips the ones already visible.
    std::array<int, kMonsterKinds> livingCounts(bool hiddenOnly) const;
    // Every monster ever placed (alive or dead) per kind.
    std::array<int, kMonsterKinds> totalCounts() const;

private:
    void flood(std::vector<Pos> seeds, std::vector<Pos>& out);
    bool expands(Pos p) const;  // revealing p opens its neighbours

    Grid<Cell> cells_;
    Topology topo_ = Topology::Square;
    std::vector<Monster> monsters_;
};

} // namespace flagd
