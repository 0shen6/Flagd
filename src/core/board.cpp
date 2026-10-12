#include "core/board.h"

#include <algorithm>

namespace flagd {

int markPower(Mark m) {
    switch (m) {
        case Mark::P1: return 1;
        case Mark::P2: return 2;
        case Mark::P3: return 3;
        case Mark::P5: return 5;
        default: return 0;
    }
}

Mark nextMark(Mark m) {
    return Mark((int(m) + 1) % int(Mark::Count));
}

int Board::addMonster(MonsterKind kind, const std::vector<Pos>& cells) {
    const MonsterDef& def = monsterDef(kind);
    Monster m;
    m.kind = kind;
    m.power = def.power;
    m.hp = def.hits;
    m.cells = cells;
    const int id = int(monsters_.size());
    monsters_.push_back(std::move(m));
    for (Pos p : cells) cells_[p].monster = int16_t(id);
    return id;
}

Monster* Board::livingMonsterAt(Pos p) {
    const int id = cells_[p].monster;
    if (id < 0 || !monsters_[size_t(id)].alive) return nullptr;
    return &monsters_[size_t(id)];
}

const Monster* Board::livingMonsterAt(Pos p) const {
    return const_cast<Board*>(this)->livingMonsterAt(p);
}

int Board::threatAt(Pos p) const {
    const int self = cells_[p].monster;
    int seen[9];
    int seenCount = 0;
    int total = 0;
    forNeighbors(width(), height(), p, [&](Pos n) {
        const int id = cells_[n].monster;
        if (id < 0 || id == self || !monsters_[size_t(id)].alive) return;
        for (int i = 0; i < seenCount; ++i)
            if (seen[i] == id) return;
        seen[seenCount++] = id;
        total += monsters_[size_t(id)].power;
    });
    return total;
}

bool Board::isHidden(Pos p) const {
    const Cell& c = cells_[p];
    if (c.revealed || c.buried) return false;
    const Monster* m = livingMonsterAt(p);
    return !(m && m->visible);
}

void Board::flood(std::vector<Pos> stack, std::vector<Pos>& out) {
    while (!stack.empty()) {
        Pos p = stack.back();
        stack.pop_back();
        Cell& c = cells_[p];
        if (c.revealed || livingMonsterAt(p)) continue;
        c.revealed = true;
        c.buried = false;
        c.mark = Mark::None;
        out.push_back(p);
        if (threatAt(p) != 0) continue;
        forNeighbors(width(), height(), p, [&](Pos n) {
            if (!cells_[n].revealed) stack.push_back(n);
        });
    }
}

std::vector<Pos> Board::revealSafe(Pos p) {
    std::vector<Pos> out;
    flood({p}, out);
    return out;
}

std::vector<Pos> Board::settleAfterKill(int monsterId) {
    std::vector<Pos> out;
    std::vector<Pos> seeds;
    for (Pos p : monsters_[size_t(monsterId)].cells) {
        cells_[p].mark = Mark::None;
        seeds.push_back(p);
    }
    flood(seeds, out);
    // Neighbours whose threat dropped to zero now open their surroundings too.
    std::vector<Pos> zeros;
    for (Pos p : monsters_[size_t(monsterId)].cells)
        forNeighbors(width(), height(), p, [&](Pos n) {
            if (cells_[n].revealed && threatAt(n) == 0) zeros.push_back(n);
        });
    for (Pos z : zeros)
        forNeighbors(width(), height(), z, [&](Pos n) {
            if (!cells_[n].revealed) flood({n}, out);
        });
    return out;
}

bool Board::allSafeRevealed() const {
    for (size_t i = 0; i < cells_.size(); ++i) {
        Pos p = cells_.posOf(i);
        if (!cells_[p].revealed && !livingMonsterAt(p)) return false;
    }
    return true;
}

std::array<int, kMonsterKinds> Board::livingCounts(bool hiddenOnly) const {
    std::array<int, kMonsterKinds> counts{};
    for (const Monster& m : monsters_)
        if (m.alive && !(hiddenOnly && m.visible)) ++counts[int(m.kind)];
    return counts;
}

} // namespace flagd
