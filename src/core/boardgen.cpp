#include "core/boardgen.h"

#include <algorithm>

#include "core/solver.h"

namespace flagd {

namespace {

bool pickCell(std::vector<Pos>& pool, Rng& rng, Pos& out) {
    if (pool.empty()) return false;
    const size_t i = size_t(rng.range(0, int(pool.size()) - 1));
    out = pool[i];
    pool.erase(pool.begin() + std::ptrdiff_t(i));
    return true;
}

Topology topologyFor(const balance::FloorParams& p) {
    if (p.boss) return Topology::Square;  // 2x2 bosses need a square grid
    if (p.modifier == Modifier::Wrap) return Topology::Wrap;
    if (p.modifier == Modifier::Hex) return Topology::Hex;
    return Topology::Square;
}

} // namespace

Board placeBoard(const balance::FloorParams& params, Pos firstClick, int safeRadius, Rng& rng) {
    Board b(params.width, params.height, topologyFor(params));
    b.fogged = params.fog;
    const int w = params.width;
    const int h = params.height;
    const auto distFromStart = b.distancesFrom(firstClick);
    auto distOf = [&](Pos p) { return distFromStart[b.grid().index(p)]; };
    auto inStartZone = [&](Pos p) { return distOf(p) <= safeRadius; };

    std::vector<char> taken(size_t(w * h), 0);
    auto idx = [&](Pos p) { return size_t(p.y * w + p.x); };

    // Bosses are 2x2, visible from the start, and kept clear of the opening.
    for (int k = 0; k < kMonsterKinds; ++k) {
        const MonsterKind kind = MonsterKind(k);
        if (!monsterDef(kind).boss) continue;
        for (int n = 0; n < params.counts[size_t(k)]; ++n) {
            // Keep the boss clear of the opening, relaxing the gap (never into
            // the opening itself) until it fits: a run must always have its boss.
            std::vector<Pos> spots;
            for (int gap = safeRadius + 2; gap >= 1 && spots.empty(); --gap)
                for (int y = 1; y + 2 < h; ++y)
                    for (int x = 1; x + 2 < w; ++x) {
                        bool ok = true;
                        for (int dy = 0; dy < 2 && ok; ++dy)
                            for (int dx = 0; dx < 2 && ok; ++dx) {
                                Pos c{x + dx, y + dy};
                                if (taken[idx(c)] || chebyshev(c, firstClick) <= gap) ok = false;
                            }
                        if (ok) spots.push_back({x, y});
                    }
            if (spots.empty()) break;
            const Pos tl = rng.pick(spots);
            std::vector<Pos> cells{tl, {tl.x + 1, tl.y}, {tl.x, tl.y + 1}, {tl.x + 1, tl.y + 1}};
            for (Pos c : cells) taken[idx(c)] = 1;
            const int id = b.addMonster(kind, cells);
            Monster& m = b.monsters()[size_t(id)];
            m.visible = true;
            m.hp += params.bossExtraHits;
        }
    }

    std::vector<Pos> pool;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            Pos p{x, y};
            if (!taken[idx(p)] && !inStartZone(p)) pool.push_back(p);
        }
    for (int k = 0; k < kMonsterKinds; ++k) {
        if (monsterDef(MonsterKind(k)).boss) continue;
        for (int n = 0; n < params.counts[size_t(k)]; ++n) {
            Pos p;
            if (!pickCell(pool, rng, p)) break;
            b.addMonster(MonsterKind(k), {p});
        }
    }

    // Features go on safe cells outside the opening. The exit prefers far cells.
    if (!params.boss) {
        int maxDist = 0;
        for (Pos p : pool) maxDist = std::max(maxDist, distOf(p));
        std::vector<Pos> far;
        for (Pos p : pool)
            if (distOf(p) * 2 >= maxDist) far.push_back(p);
        Pos exit;
        if (pickCell(far.empty() ? pool : far, rng, exit)) {
            b.at(exit).feature = Feature::Exit;
            pool.erase(std::remove(pool.begin(), pool.end(), exit), pool.end());
        }
    }
    Pos chest;
    Pos key;
    if (pickCell(pool, rng, chest) && pickCell(pool, rng, key)) {
        b.at(chest).feature = Feature::Chest;
        b.at(key).feature = Feature::Key;
    }

    // Liars: plainly marked tiles whose number is off by 1 or 2 (never shown as 0).
    for (int n = 0; n < params.liars && !pool.empty(); ++n) {
        Pos p;
        if (!pickCell(pool, rng, p)) break;
        const int t = b.threatAt(p);
        if (t == 0) continue;
        std::vector<int> offsets;
        for (int o : {-2, -1, 1, 2})
            if (t + o >= 1) offsets.push_back(o);
        b.at(p).liar = true;
        b.at(p).liarOffset = int8_t(rng.pick(offsets));
    }

    // Gold: riskier tiles (higher threat) are more likely to pay.
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            Pos p{x, y};
            Cell& c = b.at(p);
            if (c.monster >= 0 || c.feature != Feature::None) continue;
            const int t = b.threatAt(p);
            if (rng.chance(balance::goldChance(t)))
                c.gold = uint8_t(balance::goldAmount(t, rng.range(0, 1)) + (params.elite ? 1 : 0));
        }
    return b;
}

std::vector<Pos> simulateNoGuess(Board b, Pos start, Rng& rng) {
    std::vector<Pos> hints;
    b.revealSafe(start);
    while (!b.allSafeRevealed()) {
        const SolverResult r = solve(b, SolveMode::Fast);
        bool progressed = false;
        for (Pos p : r.safe)
            if (!b.at(p).revealed) {
                b.revealSafe(p);
                progressed = true;
            }
        if (progressed) continue;
        if (!r.monsters.empty()) {
            // Fighting a proven monster is a choice, not a guess: it reveals its tile.
            size_t best = 0;
            for (size_t i = 1; i < r.monsters.size(); ++i) {
                const int pi = r.monsterPower[i] ? r.monsterPower[i] : 99;
                const int pb = r.monsterPower[best] ? r.monsterPower[best] : 99;
                if (pi < pb) best = i;
            }
            const int id = b.at(r.monsters[best]).monster;
            b.monsters()[size_t(id)].alive = false;
            b.settleAfterKill(id);
            continue;
        }
        // Stuck: a real player would have to guess. Hand out a safe tile instead,
        // preferring one that touches what is already known.
        std::vector<Pos> nearKnown;
        std::vector<Pos> anySafe;
        for (int y = 0; y < b.height(); ++y)
            for (int x = 0; x < b.width(); ++x) {
                Pos p{x, y};
                if (!b.isHidden(p) || b.livingMonsterAt(p)) continue;
                anySafe.push_back(p);
                bool touches = false;
                b.forNeighbors(p, [&](Pos n) {
                    if (b.at(n).revealed) touches = true;
                });
                if (touches) nearKnown.push_back(p);
            }
        if (anySafe.empty()) break;
        const Pos hint = rng.pick(nearKnown.empty() ? anySafe : nearKnown);
        hints.push_back(hint);
        b.revealSafe(hint);
    }
    return hints;
}

GeneratedBoard generateBoard(const balance::FloorParams& params, Pos firstClick, int safeRadius,
                             uint64_t seed) {
    Rng rng(seed);
    GeneratedBoard best;
    bool have = false;
    for (int attempt = 1; attempt <= balance::kGenAttempts; ++attempt) {
        Board b = placeBoard(params, firstClick, safeRadius, rng);
        std::vector<Pos> hints = simulateNoGuess(b, firstClick, rng);
        if (!have || hints.size() < best.hints.size()) {
            best.board = std::move(b);
            best.hints = std::move(hints);
            have = true;
        }
        best.attempts = attempt;
        if (best.hints.empty()) break;
    }
    // The player starts with the hints already open. Replay from exactly that
    // state and add hints until this solver can finish it, so the guarantee
    // holds for the board as dealt.
    for (int pass = 0; pass < 8; ++pass) {
        Board start = best.board;
        start.revealSafe(firstClick);
        for (Pos p : best.hints)
            if (!start.at(p).revealed) start.revealSafe(p);
        const std::vector<Pos> extra = simulateNoGuess(start, firstClick, rng);
        if (extra.empty()) break;
        best.hints.insert(best.hints.end(), extra.begin(), extra.end());
    }
    for (Pos p : best.hints) best.board.at(p).hint = true;
    return best;
}

} // namespace flagd
