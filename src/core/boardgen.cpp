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

} // namespace

Board placeBoard(const balance::FloorParams& params, Pos firstClick, int safeRadius, Rng& rng) {
    Board b(params.width, params.height);
    const int w = params.width;
    const int h = params.height;
    auto inStartZone = [&](Pos p) { return chebyshev(p, firstClick) <= safeRadius; };

    std::vector<char> taken(size_t(w * h), 0);
    auto idx = [&](Pos p) { return size_t(p.y * w + p.x); };

    // The boss is 2x2, visible from the start, and kept clear of the opening.
    const int bossCount = params.counts[int(MonsterKind::Burrower)];
    for (int n = 0; n < bossCount; ++n) {
        std::vector<Pos> spots;
        for (int y = 1; y + 2 < h; ++y)
            for (int x = 1; x + 2 < w; ++x) {
                bool ok = true;
                for (int dy = 0; dy < 2 && ok; ++dy)
                    for (int dx = 0; dx < 2 && ok; ++dx) {
                        Pos c{x + dx, y + dy};
                        if (taken[idx(c)] || chebyshev(c, firstClick) <= safeRadius + 2) ok = false;
                    }
                if (ok) spots.push_back({x, y});
            }
        if (spots.empty()) break;
        const Pos tl = rng.pick(spots);
        std::vector<Pos> cells{tl, {tl.x + 1, tl.y}, {tl.x, tl.y + 1}, {tl.x + 1, tl.y + 1}};
        for (Pos c : cells) taken[idx(c)] = 1;
        const int id = b.addMonster(MonsterKind::Burrower, cells);
        b.monsters()[size_t(id)].visible = true;
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
        std::vector<Pos> far;
        const int minDist = std::max(w, h) / 2;
        for (Pos p : pool)
            if (chebyshev(p, firstClick) >= minDist) far.push_back(p);
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
        const SolverResult r = solve(b);
        if (!r.safe.empty()) {
            for (Pos p : r.safe)
                if (!b.at(p).revealed) b.revealSafe(p);
            continue;
        }
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
                forNeighbors(b.width(), b.height(), p, [&](Pos n) {
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
    for (Pos p : best.hints) best.board.at(p).hint = true;
    return best;
}

} // namespace flagd
