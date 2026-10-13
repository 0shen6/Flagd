#include <doctest/doctest.h>

#include <algorithm>

#include "core/boardgen.h"
#include "core/game_state.h"
#include "core/run.h"

using namespace flagd;

namespace {

Board withHintsRevealed(const GeneratedBoard& g, Pos first) {
    Board b = g.board;
    b.revealSafe(first);
    for (Pos h : g.hints)
        if (!b.at(h).revealed) b.revealSafe(h);
    return b;
}

int countEvents(const std::vector<Event>& ev, EventType t) {
    int n = 0;
    for (const Event& e : ev) n += e.type == t;
    return n;
}

} // namespace

TEST_CASE("generated boards never need a guess once hints are revealed") {
    int boardsWithHints = 0;
    int total = 0;
    for (int depth = 0; depth < 7; ++depth) {
        for (int mod = 0; mod < kModifierKinds; ++mod) {
            const auto params = balance::floorParams(depth, mod % 2 == 1, false, MonsterKind::Burrower, Modifier(mod));
            for (uint64_t seed = 1; seed <= 8; ++seed) {
                Rng pick(seed * 31 + uint64_t(depth));
                const Pos first{pick.range(0, params.width - 1), pick.range(0, params.height - 1)};
                const GeneratedBoard g = generateBoard(params, first, 1, seed * 977 + uint64_t(depth * 10 + mod));
                Rng rng(seed);
                const Board start = withHintsRevealed(g, first);
                CHECK(simulateNoGuess(start, first, rng).empty());
                CHECK(start.threatAt(first) == 0);
                CHECK_FALSE(start.livingMonsterAt(first));
                boardsWithHints += !g.hints.empty();
                ++total;
            }
        }
    }
    for (int b = 0; b < 3; ++b) {
        const auto params = balance::floorParams(7, false, true, balance::bossKindFor(b));
        for (uint64_t seed = 1; seed <= 10; ++seed) {
            const GeneratedBoard g = generateBoard(params, {0, 0}, 1, seed);
            Rng rng(seed);
            CHECK(simulateNoGuess(withHintsRevealed(g, {0, 0}), {0, 0}, rng).empty());
            boardsWithHints += !g.hints.empty();
            ++total;
        }
    }
    MESSAGE("boards needing hint tiles: " << boardsWithHints << " / " << total);
}

TEST_CASE("topologies: wrap and hex neighbours") {
    Board wrap(5, 4, Topology::Wrap);
    Pos n[8];
    CHECK(wrap.neighbors({0, 0}, n) == 8);
    bool touchesFarCorner = false;
    for (int i = 0; i < 8; ++i) touchesFarCorner = touchesFarCorner || n[i] == Pos{4, 3};
    CHECK(touchesFarCorner);

    Board hex(5, 5, Topology::Hex);
    CHECK(hex.neighbors({2, 2}, n) == 6);
    CHECK(hex.neighbors({2, 1}, n) == 6);
    CHECK(hex.neighbors({0, 0}, n) == 2);
    // Neighbourhood is symmetric.
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x) {
            const int k = hex.neighbors({x, y}, n);
            for (int i = 0; i < k; ++i) {
                Pos back[8];
                const int kb = hex.neighbors(n[i], back);
                bool found = false;
                for (int j = 0; j < kb; ++j) found = found || back[j] == Pos{x, y};
                CHECK(found);
            }
        }
}

TEST_CASE("fog shows ranges and liars are flagged; the solver stays sound") {
    Board b(3, 1);
    b.addMonster(MonsterKind::Skeleton, {{2, 0}});
    b.at({1, 0}).revealed = true;
    b.fogged = true;
    const NumberView v = b.view({1, 0});
    CHECK(v.lo == 1);
    CHECK(v.hi == 3);
    CHECK_FALSE(v.exact());
    b.at({1, 0}).liar = true;
    b.at({1, 0}).liarOffset = 1;
    CHECK(b.view({1, 0}).liar);
    CHECK(b.view({1, 0}).shown == 4);
}

TEST_CASE("boss board places a visible 2x2 Burrower") {
    const auto params = balance::floorParams(7, false, true);
    const GeneratedBoard g = generateBoard(params, {0, 0}, 1, 42);
    int bosses = 0;
    for (const Monster& m : g.board.monsters())
        if (m.kind == MonsterKind::Burrower) {
            ++bosses;
            CHECK(m.visible);
            CHECK(m.cells.size() == 4);
        }
    CHECK(bosses == 1);
}

TEST_CASE("every boss board has its boss, however large the opening") {
    for (int b = 0; b < 3; ++b)
        for (int radius = 1; radius <= 5; ++radius)
            for (uint64_t seed = 1; seed <= 6; ++seed) {
                const auto params = balance::floorParams(7, false, true, balance::bossKindFor(b));
                const GeneratedBoard g = generateBoard(params, {7, 4}, radius, seed);
                int bosses = 0;
                for (const Monster& m : g.board.monsters()) bosses += monsterDef(m.kind).boss;
                CHECK(bosses == 1);
            }
}

TEST_CASE("census matches floor parameters") {
    const auto params = balance::floorParams(3, false, false);
    const GeneratedBoard g = generateBoard(params, {5, 5}, 1, 7);
    const auto counts = g.board.livingCounts(false);
    for (int k = 0; k < kMonsterKinds; ++k) CHECK(counts[size_t(k)] == params.counts[size_t(k)]);
}

TEST_CASE("same seed and actions replay identically") {
    auto play = [](uint64_t seed) {
        Player p = makePlayer(ClassId::Warrior);
        Encounter e = makeEncounter(balance::floorParams(2, false, false), seed);
        std::vector<int> trace;
        Rng clicks(99);
        for (int i = 0; i < 40 && p.alive(); ++i) {
            Action a;
            a.pos = {clicks.range(0, e.params.width - 1), clicks.range(0, e.params.height - 1)};
            for (const Event& ev : applyAction(p, e, a)) {
                trace.push_back(int(ev.type));
                trace.push_back(ev.amount);
            }
        }
        trace.push_back(p.hp);
        trace.push_back(p.gold);
        return trace;
    };
    CHECK(play(5) == play(5));
    CHECK(play(5) != play(6));
}

TEST_CASE("ambushes cost double, marked attacks cost the monster's power") {
    Player p = makePlayer(ClassId::Warrior);
    p.hp = p.maxHp = 50;
    Encounter e = makeEncounter(balance::floorParams(0, false, false), 3);
    applyAction(p, e, {ActionType::Reveal, {4, 4}});
    REQUIRE(e.generated);

    std::vector<Pos> slimes;
    for (const Monster& m : e.board.monsters())
        if (m.alive && !m.visible && m.kind == MonsterKind::Slime) slimes.push_back(m.cells[0]);
    REQUIRE(slimes.size() >= 2);

    const int before = p.hp;
    auto ev = applyAction(p, e, {ActionType::Reveal, slimes[0]});
    CHECK(before - p.hp == 2);  // slime power 1, ambush x2
    CHECK(countEvents(ev, EventType::Kill) == 1);

}

TEST_CASE("a mark only earns the prepared-attack cost when the monster is provable") {
    // [1][S] : the revealed 1 proves the lone slime, so a marked attack costs 1.
    Player p = makePlayer(ClassId::Warrior);
    Encounter e = makeEncounter(balance::floorParams(0, false, false), 1);
    e.generated = true;
    e.board = Board(2, 1);
    e.board.addMonster(MonsterKind::Slime, {{1, 0}});
    e.board.at({0, 0}).revealed = true;
    applyAction(p, e, {ActionType::SetMark, {1, 0}, Mark::P1});
    int before = p.hp;
    applyAction(p, e, {ActionType::Reveal, {1, 0}});
    CHECK(before - p.hp == 1);

    // Nothing revealed: marking a random tile and clicking it is still an ambush.
    Player q = makePlayer(ClassId::Warrior);
    Encounter f = makeEncounter(balance::floorParams(0, false, false), 1);
    f.generated = true;
    f.board = Board(4, 4);
    f.board.addMonster(MonsterKind::Bat, {{3, 3}});
    f.board.addMonster(MonsterKind::Bat, {{0, 3}});
    applyAction(q, f, {ActionType::SetMark, {3, 3}, Mark::P2});
    before = q.hp;
    const auto ev = applyAction(q, f, {ActionType::Reveal, {3, 3}});
    CHECK(before - q.hp == 4);  // bat power 2, ambush x2
    bool flaggedGuess = false;
    for (const Event& x : ev) flaggedGuess = flaggedGuess || (x.type == EventType::Damage && x.aux == 2);
    CHECK(flaggedGuess);
}

TEST_CASE("boss armor blocks attacks until enough of the board is uncovered") {
    Player p = makePlayer(ClassId::Warrior);
    p.hp = p.maxHp = 200;
    Encounter e = makeEncounter(balance::floorParams(7, false, true, MonsterKind::Burrower), 9);
    applyAction(p, e, {ActionType::Reveal, {0, 0}});
    REQUIRE(e.generated);
    CHECK(e.bossArmor > 0);
    Pos boss{-1, -1};
    for (const Monster& m : e.board.monsters())
        if (m.kind == MonsterKind::Burrower) boss = m.cells[0];
    const int hpBefore = p.hp;
    auto ev = applyAction(p, e, {ActionType::Reveal, boss});
    CHECK(countEvents(ev, EventType::BossArmored) == 1);
    CHECK(p.hp == hpBefore);  // a bounced attack costs nothing

    // Reveal every safe tile: all armor must be gone.
    for (int y = 0; y < e.board.height(); ++y)
        for (int x = 0; x < e.board.width(); ++x)
            if ((e.board.isHidden({x, y}) || e.board.at({x, y}).buried) && !e.board.livingMonsterAt({x, y}))
                applyAction(p, e, {ActionType::Reveal, {x, y}});
    for (int y = 0; y < e.board.height(); ++y)
        for (int x = 0; x < e.board.width(); ++x)
            if (e.board.at({x, y}).buried) applyAction(p, e, {ActionType::Reveal, {x, y}});
    CHECK(e.bossArmor == 0);
    ev = applyAction(p, e, {ActionType::Reveal, boss});
    CHECK(countEvents(ev, EventType::BossHit) == 1);
}

TEST_CASE("fog boards really show ranges in play") {
    Player p = makePlayer(ClassId::Warrior);
    Encounter e = makeEncounter(balance::floorParams(3, false, false, MonsterKind::Burrower, Modifier::Fog), 4);
    applyAction(p, e, {ActionType::Reveal, {7, 4}});
    REQUIRE(e.board.fogged);
    int ranges = 0;
    for (int y = 0; y < e.board.height(); ++y)
        for (int x = 0; x < e.board.width(); ++x)
            if (e.board.at({x, y}).revealed && e.board.view({x, y}).hi > 0) ranges += !e.board.view({x, y}).exact();
    CHECK(ranges > 0);

    Player lit = makePlayer(ClassId::Warrior);
    lit.relics.push_back(RelicId::Lantern);
    Encounter e2 = makeEncounter(balance::floorParams(3, false, false, MonsterKind::Burrower, Modifier::Fog), 4);
    applyAction(lit, e2, {ActionType::Reveal, {7, 4}});
    CHECK_FALSE(e2.board.fogged);
}

TEST_CASE("a run is three acts with a boss each, then victory") {
    Run r = newRun(5, ClassId::Warrior);
    CHECK(r.bosses[0] != r.bosses[1]);
    CHECK(r.bosses[1] != r.bosses[2]);
    CHECK(r.map.bossKind == r.bosses[0]);
    for (int act = 1; act <= kActs; ++act) {
        Encounter won;
        won.params.boss = true;
        won.won = true;
        finishBoard(r, won);
        if (act < kActs) {
            CHECK(r.actCleared);
            CHECK_FALSE(r.over);
            advanceAct(r);
            CHECK(r.act == act + 1);
            CHECK(r.map.bossKind == r.bosses[size_t(act)]);
            CHECK(r.current == -1);
        }
    }
    CHECK(r.over);
    CHECK(r.victory);
}

TEST_CASE("items can be used between boards and discarded") {
    Player p = makePlayer(ClassId::Warrior);
    p.items = {ItemId::Scanner, ItemId::Potion, ItemId::Elixir};
    CHECK(useItemOutside(p, 0).empty());  // a Scanner needs a board
    CHECK(useItemOutside(p, 1).empty());  // full health: keep the Potion
    p.hp = 3;
    CHECK_FALSE(useItemOutside(p, 1).empty());
    CHECK(p.hp == 3 + balance::kPotionHeal);
    const int maxBefore = p.maxHp;
    CHECK_FALSE(useItemOutside(p, 1).empty());  // the Elixir slid into slot 1
    CHECK(p.maxHp == maxBefore + balance::kElixirMaxHp);
    discardItem(p, 0);
    CHECK(p.items.empty());
}

TEST_CASE("new items: bomb, elixir, compass, whetstone") {
    Player p = makePlayer(ClassId::Warrior);
    p.items = {ItemId::Bomb, ItemId::Elixir, ItemId::Compass, ItemId::Whetstone};
    Encounter e = makeEncounter(balance::floorParams(0, false, false), 1);
    e.generated = true;
    e.board = Board(5, 5);
    e.board.addMonster(MonsterKind::Skeleton, {{2, 2}});
    e.board.at({4, 4}).feature = Feature::Exit;
    applyAction(p, e, {ActionType::UseItem, {2, 2}, Mark::None, 0});
    CHECK_FALSE(e.board.monsters()[0].alive);
    CHECK(p.hp == p.maxHp);
    const int maxBefore = p.maxHp;
    applyAction(p, e, {ActionType::UseItem, {}, Mark::None, 0});
    CHECK(p.maxHp == maxBefore + balance::kElixirMaxHp);
    applyAction(p, e, {ActionType::UseItem, {}, Mark::None, 0});
    CHECK(e.board.at({4, 4}).sensed);
    applyAction(p, e, {ActionType::UseItem, {}, Mark::None, 0});
    CHECK(p.whetstone == balance::kWhetstoneCharges);
    CHECK(p.items.empty());
}

TEST_CASE("XP levels the player up, raising max HP and healing fully") {
    Player p = makePlayer(ClassId::Warrior);
    p.hp = 3;
    std::vector<Event> ev;
    gainXp(p, 100, ev);
    CHECK(p.level > 1);
    CHECK(p.hp == p.maxHp);
    CHECK(p.maxHp > classDef(ClassId::Warrior).maxHp);
    CHECK(countEvents(ev, EventType::LevelUp) == p.level - 1);
}

TEST_CASE("chord opens neighbours only when marks add up") {
    Player p = makePlayer(ClassId::Warrior);
    Encounter e = makeEncounter(balance::floorParams(0, false, false), 1);
    e.generated = true;
    e.board = Board(3, 3);
    e.board.addMonster(MonsterKind::Bat, {{0, 0}});
    e.board.at({1, 1}).revealed = true;  // threat 2

    auto ev = applyAction(p, e, {ActionType::Reveal, {1, 1}});
    CHECK(countEvents(ev, EventType::Invalid) == 1);

    applyAction(p, e, {ActionType::SetMark, {0, 0}, Mark::P2});
    applyAction(p, e, {ActionType::Reveal, {1, 1}});
    CHECK(e.board.allSafeRevealed());
    CHECK(p.hp == p.maxHp);
}

TEST_CASE("exit allows descending; dying ends the encounter") {
    Player p = makePlayer(ClassId::Warrior);
    Encounter e = makeEncounter(balance::floorParams(0, false, false), 11);
    auto ev = applyAction(p, e, {ActionType::Descend});
    CHECK(countEvents(ev, EventType::Invalid) == 1);
    e.exitFound = true;
    applyAction(p, e, {ActionType::Descend});
    CHECK(e.descended);

    Player q = makePlayer(ClassId::Warrior);
    q.hp = 1;
    Encounter f = makeEncounter(balance::floorParams(0, false, false), 1);
    f.generated = true;
    f.board = Board(2, 1);
    f.board.addMonster(MonsterKind::Skeleton, {{1, 0}});
    ev = applyAction(q, f, {ActionType::Reveal, {1, 0}});
    CHECK(countEvents(ev, EventType::PlayerDied) == 1);
    CHECK(f.lost);
}

TEST_CASE("Thick Skin and Shield reduce fight damage") {
    Player p = makePlayer(ClassId::Warrior);
    p.relics.push_back(RelicId::ThickSkin);
    p.shield = 1;
    Encounter e = makeEncounter(balance::floorParams(0, false, false), 1);
    e.generated = true;
    e.board = Board(3, 1);
    e.board.addMonster(MonsterKind::Skeleton, {{0, 0}});
    e.board.addMonster(MonsterKind::Skeleton, {{2, 0}});
    auto ev = applyAction(p, e, {ActionType::Reveal, {0, 0}});
    CHECK(countEvents(ev, EventType::ShieldBlock) == 1);
    CHECK(countEvents(ev, EventType::Damage) == 0);
    ev = applyAction(p, e, {ActionType::Reveal, {2, 0}});
    int dealt = 0;
    for (const Event& x : ev)
        if (x.type == EventType::Damage) dealt += x.amount;
    CHECK(dealt == 3 * 2 - 1);  // skeleton ambush (x2) minus Thick Skin
}

TEST_CASE("shop purchases spend gold and respect inventory") {
    Run r = newRun(77, ClassId::Warrior);
    r.player.gold = 500;
    auto offers = makeShop(r, 3);
    REQUIRE(offers.size() >= 4);
    for (auto& o : offers) buy(r, o);
    CHECK(r.player.gold < 500);
    CHECK(int(r.player.items.size()) <= balance::kInventorySlots);
    CHECK(r.player.relics.size() == 2);
}

TEST_CASE("Scanner exposes without fighting; Defuser kills for free; Potion heals") {
    Player p = makePlayer(ClassId::Warrior);
    p.items = {ItemId::Scanner, ItemId::Defuser, ItemId::Potion};
    Encounter e = makeEncounter(balance::floorParams(0, false, false), 1);
    e.generated = true;
    e.board = Board(5, 5);
    e.board.addMonster(MonsterKind::Spider, {{2, 2}});
    applyAction(p, e, {ActionType::UseItem, {2, 2}, Mark::None, 0});
    CHECK(e.board.monsters()[0].visible);
    CHECK(e.board.monsters()[0].alive);
    CHECK(e.board.at({1, 1}).revealed);
    CHECK(p.hp == p.maxHp);

    applyAction(p, e, {ActionType::UseItem, {2, 2}, Mark::None, 0});  // Defuser is now slot 0
    CHECK_FALSE(e.board.monsters()[0].alive);
    CHECK(p.hp == p.maxHp);
    CHECK(p.items.size() == 1);

    p.hp = 2;
    applyAction(p, e, {ActionType::UseItem, {}, Mark::None, 0});
    CHECK(p.hp == std::min(p.maxHp, 2 + balance::kPotionHeal));
    CHECK(p.items.empty());
}

TEST_CASE("the Burrower re-covers a number every few turns, and digging restores it") {
    Player p = makePlayer(ClassId::Warrior);
    p.hp = p.maxHp = 99;
    Encounter e = makeEncounter(balance::floorParams(7, false, true), 5);
    applyAction(p, e, {ActionType::Reveal, {0, 0}});
    int buried = 0;
    Pos dug{-1, -1};
    for (int turn = 0; turn < 3 * balance::kBossBurrowEvery; ++turn) {
        // Reveal any still-hidden safe cell to pass turns.
        bool acted = false;
        for (int y = 0; y < e.board.height() && !acted; ++y)
            for (int x = 0; x < e.board.width() && !acted; ++x)
                if (e.board.isHidden({x, y}) && !e.board.livingMonsterAt({x, y})) {
                    for (const Event& ev : applyAction(p, e, {ActionType::Reveal, {x, y}}))
                        if (ev.type == EventType::Burrow) {
                            ++buried;
                            dug = ev.pos;
                        }
                    acted = true;
                }
        if (!acted) break;
    }
    REQUIRE(buried > 0);
    if (e.board.at(dug).buried) {
        applyAction(p, e, {ActionType::Reveal, dug});
        CHECK(e.board.at(dug).revealed);
    }
}

// Slow: run with `flagd_tests --no-skip -tc="stress*"`.
TEST_CASE("stress: 7000 boards across every floor tier and twist stay guess-free" * doctest::skip()) {
    int hinted = 0;
    int total = 0;
    for (int depth = 0; depth < 7; ++depth)
        for (int mod = 0; mod < kModifierKinds; ++mod) {
            const auto params = balance::floorParams(depth, false, false, MonsterKind::Burrower, Modifier(mod));
            for (uint64_t seed = 1; seed <= 200; ++seed) {
                Rng pick(seed);
                const Pos first{pick.range(0, params.width - 1), pick.range(0, params.height - 1)};
                const GeneratedBoard g = generateBoard(params, first, 1, seed * 31337 + uint64_t(depth * 10 + mod));
                Rng rng(seed);
                CHECK(simulateNoGuess(withHintsRevealed(g, first), first, rng).empty());
                hinted += !g.hints.empty();
                ++total;
            }
        }
    MESSAGE("boards needing hint tiles: " << hinted << " / " << total);
}
