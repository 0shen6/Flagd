#include <doctest/doctest.h>

#include "core/board.h"

using namespace flagd;

TEST_CASE("flood fill opens an empty board completely") {
    Board b(5, 4);
    auto opened = b.revealSafe({2, 2});
    CHECK(opened.size() == 20);
    CHECK(b.allSafeRevealed());
}

TEST_CASE("threat sums distinct living monsters, counting a 2x2 boss once") {
    Board b(6, 6);
    b.addMonster(MonsterKind::Bat, {{0, 0}});                                  // power 2
    b.addMonster(MonsterKind::Burrower, {{2, 2}, {3, 2}, {2, 3}, {3, 3}});    // power 8
    CHECK(b.threatAt({1, 1}) == 2 + 8);
    CHECK(b.threatAt({4, 4}) == 8);
    CHECK(b.threatAt({2, 2}) == 0);  // a boss cell never counts itself
    b.monsters()[0].alive = false;
    CHECK(b.threatAt({1, 1}) == 8);
}

TEST_CASE("flood stops at numbers and never opens monsters") {
    Board b(5, 1);
    b.addMonster(MonsterKind::Slime, {{4, 0}});
    auto opened = b.revealSafe({0, 0});
    CHECK(opened.size() == 4);
    CHECK_FALSE(b.at({4, 0}).revealed);
    CHECK(b.threatAt({3, 0}) == 1);
    CHECK(b.allSafeRevealed());
}

TEST_CASE("a kill reveals the monster's cell and cascades through new zeros") {
    Board b(5, 1);
    b.addMonster(MonsterKind::Slime, {{2, 0}});
    b.revealSafe({0, 0});
    CHECK_FALSE(b.at({3, 0}).revealed);
    b.monsters()[0].alive = false;
    b.settleAfterKill(0);
    CHECK(b.at({2, 0}).revealed);
    CHECK(b.at({4, 0}).revealed);
}

TEST_CASE("marks cycle through flag and power marks") {
    Mark m = Mark::None;
    m = nextMark(m);
    CHECK(m == Mark::Flag);
    m = nextMark(m);
    CHECK(markPower(m) == 1);
    for (int i = 0; i < 4; ++i) m = nextMark(m);
    CHECK(m == Mark::None);
}
