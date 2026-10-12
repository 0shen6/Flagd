#include <doctest/doctest.h>

#include <algorithm>
#include <functional>

#include "core/boardgen.h"
#include "core/rng.h"
#include "core/solver.h"

using namespace flagd;

namespace {

bool contains(const std::vector<Pos>& v, Pos p) { return std::find(v.begin(), v.end(), p) != v.end(); }

// Brute force: every assignment of hidden cells matching numbers and census.
// Returns for each hidden cell whether it can be safe / can be a monster.
void bruteForce(const Board& b, std::vector<Pos>& hidden, std::vector<bool>& canSafe,
                std::vector<bool>& canMonster) {
    hidden.clear();
    for (int y = 0; y < b.height(); ++y)
        for (int x = 0; x < b.width(); ++x)
            if (b.isHidden({x, y})) hidden.push_back({x, y});
    std::vector<int> powers;
    std::vector<int> counts;
    auto census = b.livingCounts(true);
    for (int k = 0; k < kMonsterKinds; ++k)
        if (census[size_t(k)]) {
            powers.push_back(monsterDef(MonsterKind(k)).power);
            counts.push_back(census[size_t(k)]);
        }
    canSafe.assign(hidden.size(), false);
    canMonster.assign(hidden.size(), false);
    std::vector<int> value(hidden.size(), 0);
    auto consistent = [&]() {
        for (int y = 0; y < b.height(); ++y)
            for (int x = 0; x < b.width(); ++x) {
                Pos p{x, y};
                if (!b.at(p).revealed) continue;
                int sum = 0;
                forNeighbors(b.width(), b.height(), p, [&](Pos n) {
                    for (size_t i = 0; i < hidden.size(); ++i)
                        if (hidden[i] == n) sum += value[i];
                    const Monster* m = b.livingMonsterAt(n);
                    if (m && m->visible) sum += m->power;
                });
                if (sum != b.threatAt(p)) return false;
            }
        return true;
    };
    std::function<void(size_t)> rec = [&](size_t i) {
        if (i == hidden.size()) {
            for (int c : counts)
                if (c != 0) return;
            if (!consistent()) return;
            for (size_t j = 0; j < hidden.size(); ++j) {
                if (value[j] == 0) canSafe[j] = true;
                else canMonster[j] = true;
            }
            return;
        }
        value[i] = 0;
        rec(i + 1);
        for (size_t k = 0; k < powers.size(); ++k) {
            if (counts[k] == 0) continue;
            --counts[k];
            value[i] = powers[k];
            rec(i + 1);
            ++counts[k];
        }
        value[i] = 0;
    };
    rec(0);
}

} // namespace

TEST_CASE("solver finds a lone monster behind a number") {
    Board b(3, 1);
    b.addMonster(MonsterKind::Bat, {{2, 0}});
    b.revealSafe({0, 0});
    const auto r = solve(b);
    REQUIRE(r.monsters.size() == 1);
    CHECK(r.monsters[0] == Pos{2, 0});
    CHECK(r.monsterPower[0] == 2);
    CHECK(r.safe.empty());
}

TEST_CASE("solver uses sums: a 3 between two cells with powers {1,2} marks both") {
    // Row: [M?][3][M?] with census Slime(1) + Bat(2): both cells must be monsters.
    Board b(3, 1);
    b.addMonster(MonsterKind::Slime, {{0, 0}});
    b.addMonster(MonsterKind::Bat, {{2, 0}});
    b.at({1, 0}).revealed = true;
    const auto r = solve(b);
    CHECK(r.monsters.size() == 2);
    CHECK(r.safe.empty());
}

TEST_CASE("solver deductions are sound against brute force on random small boards") {
    Rng rng(12345);
    int checkedSafe = 0;
    int checkedMonsters = 0;
    for (int trial = 0; trial < 400; ++trial) {
        Board b(4, 4);
        std::vector<Pos> cells;
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) cells.push_back({x, y});
        rng.shuffle(cells);
        const int monsters = rng.range(1, 4);
        for (int i = 0; i < monsters; ++i)
            b.addMonster(MonsterKind(rng.range(0, 2)), {cells[size_t(i)]});
        const int reveals = rng.range(1, 6);
        for (int i = 0; i < reveals; ++i) {
            Pos p = cells[size_t(monsters + rng.range(0, 15 - monsters))];
            if (!b.at(p).revealed) b.revealSafe(p);
        }
        if (rng.chance(0.3)) b.monsters()[0].visible = true;

        std::vector<Pos> hidden;
        std::vector<bool> canSafe;
        std::vector<bool> canMonster;
        bruteForce(b, hidden, canSafe, canMonster);
        const auto r = solve(b);
        for (size_t i = 0; i < hidden.size(); ++i) {
            if (contains(r.safe, hidden[i])) {
                CHECK_FALSE(canMonster[i]);
                ++checkedSafe;
            }
            if (contains(r.monsters, hidden[i])) {
                CHECK_FALSE(canSafe[i]);
                ++checkedMonsters;
            }
        }
        // Completeness for small boards: anything brute force proves, the solver proves.
        for (size_t i = 0; i < hidden.size(); ++i) {
            if (!canMonster[i]) CHECK(contains(r.safe, hidden[i]));
        }
    }
    CHECK(checkedSafe > 50);
    CHECK(checkedMonsters > 50);
}
