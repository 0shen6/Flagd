// Autoplay: a bot with perfect deduction plays whole runs. It checks runs
// finish without getting stuck and reports balance numbers.
#include <doctest/doctest.h>

#include <algorithm>
#include <map>
#include <string>

#include "core/content/balance.h"
#include "core/run.h"
#include "core/solver.h"

using namespace flagd;

namespace {

struct BotStats {
    int wins = 0;
    int runs = 0;
    std::map<int, int> deathsByLayer;
    long levelAtBoss = 0;
    int reachedBoss = 0;
    std::map<int, std::pair<int, int>> byBoss;  // boss kind -> (wins, fights)
};

Mark markFor(int power) {
    switch (power) {
        case 1: return Mark::P1;
        case 2: return Mark::P2;
        case 3: return Mark::P3;
        case 5: return Mark::P5;
        default: return Mark::Flag;
    }
}

// Plays one board. Returns when the bot descends, wins, or dies.
void playBoard(Player& pl, Encounter& enc, bool marks) {
    applyAction(pl, enc, {ActionType::Reveal, {enc.params.width / 2, enc.params.height / 2}});
    for (int guard = 0; guard < 5000 && !enc.lost && !enc.won && !enc.descended; ++guard) {
        Board& b = enc.board;
        // Open the chest once the key is in hand.
        bool acted = false;
        for (int y = 0; y < b.height() && !acted; ++y)
            for (int x = 0; x < b.width() && !acted; ++x) {
                const Cell& c = b.at({x, y});
                if (enc.hasKey && c.revealed && c.feature == Feature::Chest && !c.chestOpen) {
                    applyAction(pl, enc, {ActionType::Reveal, {x, y}});
                    acted = true;
                }
            }
        if (acted) continue;

        const SolverResult r = solve(b);
        if (!r.safe.empty()) {
            applyAction(pl, enc, {ActionType::Reveal, r.safe[0]});
            continue;
        }
        // Drink a potion when low.
        if (pl.hp * 2 < pl.maxHp) {
            auto it = std::find(pl.items.begin(), pl.items.end(), ItemId::Potion);
            if (it != pl.items.end()) {
                applyAction(pl, enc, {ActionType::UseItem, {}, Mark::None, int(it - pl.items.begin())});
                continue;
            }
        }
        // Pick a survivable fight. Best: the biggest one whose XP triggers a
        // level-up (the full heal refunds it). Otherwise the cheapest.
        Pos target{-1, -1};
        int bestScore = -(1 << 30);
        const int need = balance::xpToNext(pl.level) - pl.xp;
        auto consider = [&](Pos p, int cost, int xp) {
            if (!marks && b.isHidden(p)) cost *= 2;  // unmarked fights are ambushes
            if (cost >= pl.hp) return;
            const int gained = std::max(xp, xp * classDef(pl.cls).xpPercent / 100);
            const int score = gained >= need ? 1000 + cost : -cost;
            if (score > bestScore) {
                bestScore = score;
                target = p;
            }
        };
        for (size_t i = 0; i < r.monsters.size(); ++i) {
            const int power = r.monsterPower[i] ? r.monsterPower[i] : 5;  // assume the worst
            consider(r.monsters[i], power, r.monsterPower[i] ? power : 1);
        }
        for (const Monster& m : b.monsters())
            if (m.alive && m.visible) consider(m.cells[0], monsterDef(m.kind).hitCost, monsterDef(m.kind).xp);
        auto slotOf = [&](ItemId id) {
            auto it = std::find(pl.items.begin(), pl.items.end(), id);
            return it == pl.items.end() ? -1 : int(it - pl.items.begin());
        };
        if (target.x >= 0) {
            const Monster* tm = b.livingMonsterAt(target);
            const int cost = tm ? monsterDef(tm->kind).hitCost : 0;
            // Spend items on the big fights: a Defuser is a free kill (or boss hit),
            // a Shield makes the next fight free.
            if (cost >= 3 && slotOf(ItemId::Defuser) >= 0 && !b.isHidden(target)) {
                applyAction(pl, enc, {ActionType::UseItem, target, Mark::None, slotOf(ItemId::Defuser)});
                continue;
            }
            if (cost >= 3 && slotOf(ItemId::Shield) >= 0 && pl.shield == 0) {
                applyAction(pl, enc, {ActionType::UseItem, {}, Mark::None, slotOf(ItemId::Shield)});
                continue;
            }
            const Cell& c = b.at(target);
            if (marks && b.isHidden(target) && c.mark == Mark::None) {
                int power = 0;
                for (size_t i = 0; i < r.monsters.size(); ++i)
                    if (r.monsters[i] == target) power = r.monsterPower[i];
                applyAction(pl, enc, {ActionType::SetMark, target, markFor(power)});
            }
            applyAction(pl, enc, {ActionType::Reveal, target});
            continue;
        }
        if (enc.exitFound) {
            applyAction(pl, enc, {ActionType::Descend});
            continue;
        }
        // Stuck: nothing safe, nothing affordable, no exit. Scan the frontier,
        // drink anything, then fight anyway.
        if (slotOf(ItemId::Scanner) >= 0) {
            Pos spot{-1, -1};
            for (int y = 0; y < b.height() && spot.x < 0; ++y)
                for (int x = 0; x < b.width() && spot.x < 0; ++x)
                    if (b.isHidden({x, y})) spot = {x, y};
            if (spot.x >= 0) {
                applyAction(pl, enc, {ActionType::UseItem, spot, Mark::None, slotOf(ItemId::Scanner)});
                continue;
            }
        }
        if (!pl.items.empty() && pl.hp < pl.maxHp) {
            auto it = std::find(pl.items.begin(), pl.items.end(), ItemId::Potion);
            if (it != pl.items.end()) {
                applyAction(pl, enc, {ActionType::UseItem, {}, Mark::None, int(it - pl.items.begin())});
                continue;
            }
        }
        int worst = 1 << 30;
        for (const Monster& m : b.monsters())
            if (m.alive && monsterDef(m.kind).hitCost < worst) {
                worst = monsterDef(m.kind).hitCost;
                target = m.cells[0];
            }
        if (target.x < 0) break;
        if (b.isHidden(target)) applyAction(pl, enc, {ActionType::SetMark, target, Mark::Flag});
        applyAction(pl, enc, {ActionType::Reveal, target});
    }
}

bool playRun(uint64_t seed, BotStats& st, bool marks, ClassId cls = ClassId::Warrior) {
    Run run = newRun(seed, cls);
    run.player.relicPool = (1u << kRelicKinds) - 1;
    Rng rng(seed ^ 0xB07);
    while (!run.over) {
        const auto choices = run.choices();
        REQUIRE_FALSE(choices.empty());
        // Prefer fights when healthy, recovery when hurt.
        const bool hurt = run.player.hp * 100 < run.player.maxHp * 55;
        int pick = choices[0];
        int bestScore = -1000;
        for (int id : choices) {
            int score = rng.range(0, 3);
            switch (run.node(id).type) {
                case NodeType::Normal: score += hurt ? 2 : 6; break;
                case NodeType::Elite: score += hurt ? -4 : 5; break;
                case NodeType::Rest: score += hurt ? 8 : 1; break;
                case NodeType::Shop: score += run.player.gold >= 30 ? 5 : 0; break;
                case NodeType::Treasure: score += 6; break;
                case NodeType::Event: score += 3; break;
                case NodeType::Boss: score += 10; break;
            }
            if (score > bestScore) {
                bestScore = score;
                pick = id;
            }
        }
        REQUIRE(travel(run, pick));
        const MapNode& n = run.node(pick);
        if (isBoardNode(n.type)) {
            if (n.type == NodeType::Boss) {
                ++st.reachedBoss;
                st.levelAtBoss += run.player.level;
                ++st.byBoss[int(run.map.bossKind)].second;
            }
            Encounter enc = beginEncounter(run, pick);
            playBoard(run.player, enc, marks);
            REQUIRE((enc.lost || enc.won || enc.descended));
            finishBoard(run, enc);
            if (enc.lost) ++st.deathsByLayer[n.layer];
        } else if (n.type == NodeType::Rest) {
            if (run.player.hp * 100 < run.player.maxHp * 70) restHeal(run.player);
            else restFortify(run.player);
        } else if (n.type == NodeType::Shop) {
            auto offers = makeShop(run, pick);
            for (auto& o : offers) buy(run, o);
        } else if (n.type == NodeType::Treasure) {
            takeTreasure(run, pick);
        } else if (n.type == NodeType::Event) {
            const EventId e = eventAt(run, pick);
            resolveEvent(run, pick, eventOptionAllowed(run, e, 0) && run.player.hp * 2 > run.player.maxHp ? 0 : 1);
        }
    }
    ++st.runs;
    st.wins += run.victory;
    if (run.victory) ++st.byBoss[int(run.map.bossKind)].first;
    return run.victory;
}

std::string report(const BotStats& st) {
    std::string deaths;
    for (const auto& [layer, n] : st.deathsByLayer) deaths += " F" + std::to_string(layer + 1) + ":" + std::to_string(n);
    return "wins " + std::to_string(st.wins) + "/" + std::to_string(st.runs) + ", deaths by floor:" + deaths +
           ", avg level at boss " +
           std::to_string(st.reachedBoss ? double(st.levelAtBoss) / st.reachedBoss : 0.0).substr(0, 4) + ", bosses:" +
           [&] {
               std::string b;
               for (const auto& [k, wf] : st.byBoss)
                   b += std::string(" ") + monsterDef(MonsterKind(k)).name + " " + std::to_string(wf.first) + "/" +
                        std::to_string(wf.second);
               return b;
           }();
}

} // namespace

TEST_CASE("autoplay: runs always finish; careful play beats sloppy play") {
    BotStats careful;
    BotStats sloppy;
    for (uint64_t seed = 1; seed <= 60; ++seed) {
        playRun(seed * 7919, careful, true);
        playRun(seed * 7919, sloppy, false);
    }
    MESSAGE("careful bot (marks monsters): " << report(careful));
    MESSAGE("sloppy bot (never marks):     " << report(sloppy));
    CHECK(careful.runs == 60);
    CHECK(sloppy.runs == 60);
    // A perfect deducer should win most runs; skipping marks should hurt.
    CHECK(careful.wins >= careful.runs / 2);
    CHECK(careful.wins > sloppy.wins);
}

TEST_CASE("autoplay: every class can win" * doctest::skip()) {
    for (int c = 0; c < kClassKinds; ++c) {
        BotStats st;
        for (uint64_t seed = 1; seed <= 60; ++seed) playRun(seed * 7919, st, true, ClassId(c));
        MESSAGE(std::string(classDef(ClassId(c)).name) << ": " << report(st));
        CHECK(st.wins > 0);
    }
}
