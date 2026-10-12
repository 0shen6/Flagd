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
    m.visible = def.trait == Trait::Wanderer;  // wanderers are always in plain sight
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

int Board::neighbors(Pos p, Pos out[8]) const {
    const int w = width();
    const int h = height();
    int k = 0;
    auto add = [&](Pos n) {
        if (n == p) return;
        for (int i = 0; i < k; ++i)
            if (out[i] == n) return;
        out[k++] = n;
    };
    switch (topo_) {
        case Topology::Square:
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    const Pos n{p.x + dx, p.y + dy};
                    if (n.x >= 0 && n.y >= 0 && n.x < w && n.y < h) add(n);
                }
            break;
        case Topology::Wrap:
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) add({(p.x + dx + w) % w, (p.y + dy + h) % h});
            break;
        case Topology::Hex: {
            static constexpr int kEven[6][2] = {{-1, 0}, {1, 0}, {-1, -1}, {0, -1}, {-1, 1}, {0, 1}};
            static constexpr int kOdd[6][2] = {{-1, 0}, {1, 0}, {0, -1}, {1, -1}, {0, 1}, {1, 1}};
            const auto& d = (p.y & 1) ? kOdd : kEven;
            for (const auto& o : d) {
                const Pos n{p.x + o[0], p.y + o[1]};
                if (n.x >= 0 && n.y >= 0 && n.x < w && n.y < h) add(n);
            }
            break;
        }
    }
    return k;
}

std::vector<int> Board::distancesFrom(Pos p) const {
    std::vector<int> dist(cells_.size(), -1);
    std::vector<Pos> queue{p};
    dist[cells_.index(p)] = 0;
    for (size_t i = 0; i < queue.size(); ++i) {
        const Pos q = queue[i];
        const int d = dist[cells_.index(q)];
        forNeighbors(q, [&](Pos n) {
            int& dn = dist[cells_.index(n)];
            if (dn >= 0) return;
            dn = d + 1;
            queue.push_back(n);
        });
    }
    return dist;
}

std::vector<Pos> Board::within(Pos p, int radius) const {
    std::vector<Pos> out{p};
    if (radius <= 0) return out;
    const auto dist = distancesFrom(p);
    for (size_t i = 0; i < dist.size(); ++i)
        if (dist[i] > 0 && dist[i] <= radius) out.push_back(cells_.posOf(i));
    return out;
}

int Board::threatAt(Pos p) const {
    const int self = cells_[p].monster;
    int seen[8];
    int seenCount = 0;
    int total = 0;
    forNeighbors(p, [&](Pos n) {
        const int id = cells_[n].monster;
        if (id < 0 || id == self || !monsters_[size_t(id)].alive) return;
        for (int i = 0; i < seenCount; ++i)
            if (seen[i] == id) return;
        seen[seenCount++] = id;
        total += monsters_[size_t(id)].power;
    });
    return total;
}

NumberView Board::view(Pos p) const {
    const Cell& c = cells_[p];
    const int t = threatAt(p);
    if (c.liar) {
        const int shown = std::max(0, t + c.liarOffset);
        return {0, 99, shown, true};
    }
    if (fogged && !c.clearFog && t > 0) {
        static constexpr int kBuckets[][2] = {{1, 3}, {4, 6}, {7, 9}, {10, 14}, {15, 99}};
        for (const auto& bk : kBuckets)
            if (t >= bk[0] && t <= bk[1]) return {bk[0], bk[1], t, false};
    }
    return {t, t, t, false};
}

bool Board::isHidden(Pos p) const {
    const Cell& c = cells_[p];
    if (c.revealed || c.buried) return false;
    const Monster* m = livingMonsterAt(p);
    return !(m && m->visible);
}

bool Board::surrounded(Pos p) const {
    bool all = true;
    forNeighbors(p, [&](Pos n) {
        if (isHidden(n) || cells_[n].buried) all = false;
    });
    return all;
}

bool Board::expands(Pos p) const {
    return !cells_[p].liar && threatAt(p) == 0;
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
        if (!expands(p)) continue;
        forNeighbors(p, [&](Pos n) {
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
        forNeighbors(p, [&](Pos n) {
            if (cells_[n].revealed && expands(n)) zeros.push_back(n);
        });
    for (Pos z : zeros)
        forNeighbors(z, [&](Pos n) {
            if (!cells_[n].revealed) flood({n}, out);
        });
    return out;
}

void Board::moveMonster(int monsterId, Pos to) {
    Monster& m = monsters_[size_t(monsterId)];
    const Pos from = m.cells[0];
    cells_[from].monster = -1;
    cells_[to].monster = int16_t(monsterId);
    cells_[to].mark = Mark::None;
    m.cells[0] = to;
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

std::array<int, kMonsterKinds> Board::totalCounts() const {
    std::array<int, kMonsterKinds> counts{};
    for (const Monster& m : monsters_) ++counts[int(m.kind)];
    return counts;
}

} // namespace flagd
