#include <doctest/doctest.h>

#include "core/meta.h"

using namespace flagd;

TEST_CASE("meta save round-trips") {
    Meta m;
    m.embers = 42;
    m.maxHeat = 2;
    m.classes |= 1u << int(ClassId::Seer);
    m.kills[3] = 7;
    m.upgrades[1] = 2;
    Meta back;
    REQUIRE(loadMeta(saveMeta(m), back));
    CHECK(back.embers == 42);
    CHECK(back.maxHeat == 2);
    CHECK(classUnlocked(back, ClassId::Seer));
    CHECK(back.kills[3] == 7);
    CHECK(back.upgrades[1] == 2);
    Meta junk;
    CHECK_FALSE(loadMeta("not a save", junk));
}

TEST_CASE("upgrades and relic unlocks cost embers and apply to new runs") {
    Meta m;
    m.embers = 200;
    CHECK(buyUpgrade(m, UpgradeId::Vitality));
    CHECK(buyUpgrade(m, UpgradeId::Purse));
    CHECK(buyUpgrade(m, UpgradeId::Prepared));
    CHECK_FALSE(buyUpgrade(m, UpgradeId::Prepared));  // maxed
    CHECK(buyRelicUnlock(m, RelicId::GasMask));
    CHECK_FALSE(buyRelicUnlock(m, RelicId::GasMask));
    const Run r = startRun(m, 1, ClassId::Warrior, 0);
    CHECK(r.player.maxHp == classDef(ClassId::Warrior).maxHp + 1);
    CHECK(r.player.gold == 10);
    CHECK(r.player.items.size() == 2);
    CHECK(((r.player.relicPool >> int(RelicId::GasMask)) & 1u) == 1u);
    CHECK(((r.player.relicPool >> int(RelicId::Lantern)) & 1u) == 0u);
}

TEST_CASE("recording runs awards embers and unlocks classes and heat") {
    Meta m;
    Run r = newRun(3, ClassId::Warrior, 0);
    r.deepestLayer = 7;
    r.victory = true;
    r.player.goldEarned = 200;
    const RunResult res = recordRun(m, r);
    CHECK(res.embers > 20);
    CHECK(classUnlocked(m, ClassId::Sapper));
    CHECK(classUnlocked(m, ClassId::Seer));
    CHECK(classUnlocked(m, ClassId::Gambler));
    CHECK(m.maxHeat == 1);
    CHECK(m.wins == 1);
    CHECK(res.unlocks.size() == 4);
}

TEST_CASE("events resolve deterministically and respect costs") {
    Run r = newRun(11, ClassId::Warrior, 0);
    for (int id = 0; id < int(r.map.nodes.size()); ++id) {
        Run copy = r;
        const EventId e = eventAt(copy, id);
        if (!eventOptionAllowed(copy, e, 0)) continue;
        const std::string a = resolveEvent(copy, id, 0);
        Run again = r;
        CHECK(resolveEvent(again, id, 0) == a);
        CHECK_FALSE(a.empty());
        CHECK(copy.player.hp > 0);
    }
    Run poor = r;
    poor.player.gold = 0;
    CHECK_FALSE(eventOptionAllowed(poor, EventId::Gamble, 0));
    CHECK((resolveEvent(poor, 0, 0).empty() || eventAt(poor, 0) != EventId::Gamble));
}
