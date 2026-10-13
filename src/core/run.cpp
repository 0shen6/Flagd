#include "core/run.h"

#include <algorithm>

namespace flagd {

std::vector<int> Run::choices() const {
    if (over) return {};
    if (current < 0) return map.layer(0);
    return map.nodes[size_t(current)].next;
}

namespace {

Map mapForAct(const Run& r) {
    return generateMap(deriveSeed(r.seed, uint64_t(r.act) * 7919), r.heat, r.act, r.bosses[size_t(r.act - 1)]);
}

} // namespace

Run newRun(uint64_t seed, ClassId cls, int heat) {
    Run r;
    r.seed = seed;
    r.heat = heat;
    r.player = makePlayer(cls);
    r.player.halfLevelHeal = heat >= 3;
    std::vector<int> order{0, 1, 2};
    Rng rng(deriveSeed(seed, 0xB055));
    rng.shuffle(order);
    for (int i = 0; i < kActs; ++i) r.bosses[size_t(i)] = balance::bossKindFor(order[size_t(i) % 3]);
    r.map = mapForAct(r);
    return r;
}

int floorIndex(const Run& run, int layer) { return (run.act - 1) * run.map.layers + layer; }

std::vector<RelicId> bossRelicChoices(const Run& run) {
    Rng rng(deriveSeed(run.seed, 0xB0550 + uint64_t(run.act)));
    auto pool = availableRelics(run.player);
    rng.shuffle(pool);
    if (pool.size() > 3) pool.resize(3);
    return pool;
}

void advanceAct(Run& run) {
    if (run.act >= kActs) return;
    ++run.act;
    run.actCleared = false;
    run.map = mapForAct(run);
    run.current = -1;
    run.path.clear();
    Player& p = run.player;
    const int percent = run.heat >= 3 ? balance::kActHealPercent / 2 : balance::kActHealPercent;
    p.hp = std::min(p.maxHp, p.hp + (p.maxHp - p.hp) * percent / 100);
    p.poison = 0;
}

bool travel(Run& run, int nodeId) {
    const auto opts = run.choices();
    if (std::find(opts.begin(), opts.end(), nodeId) == opts.end()) return false;
    run.current = nodeId;
    run.path.push_back(nodeId);
    run.deepestLayer = std::max(run.deepestLayer, floorIndex(run, run.node(nodeId).layer));
    return true;
}

Encounter makeEncounterFor(const Run& run, int nodeId) {
    const MapNode& n = run.node(nodeId);
    const auto params = balance::floorParams(n.layer, n.type == NodeType::Elite, n.type == NodeType::Boss,
                                             run.map.bossKind, n.modifier, run.heat, run.act);
    return makeEncounter(params, deriveSeed(run.seed, uint64_t(run.act) * 1000 + uint64_t(nodeId) + 1));
}

Encounter beginEncounter(Run& run, int nodeId) {
    Encounter e = makeEncounterFor(run, nodeId);
    e.bonusRadius = run.nextBonusRadius;
    e.hexed = run.nextHexed;
    run.nextBonusRadius = 0;
    run.nextHexed = false;
    return e;
}

void finishBoard(Run& run, const Encounter& enc) {
    if (enc.lost) {
        run.over = true;
        return;
    }
    ++run.player.floorsCleared;
    if (enc.won && enc.params.boss) {
        run.bossesBeaten |= 1u << int(run.map.bossKind);
        if (run.act < kActs) {
            run.actCleared = true;
        } else {
            run.over = true;
            run.victory = true;
        }
    }
}

std::vector<ShopOffer> makeShop(const Run& run, int nodeId) {
    Rng rng(deriveSeed(run.seed, 0x5000 + uint64_t(run.act) * 100 + uint64_t(nodeId)));
    auto price = [&](int base) {
        return balance::shopPrice(base, run.heat, run.act) * (100 - run.player.shopDiscount) / 100;
    };
    std::vector<ShopOffer> offers;
    std::vector<int> items;
    for (int i = 0; i < kItemKinds; ++i) items.push_back(i);
    rng.shuffle(items);
    for (int i = 0; i < 3; ++i) {
        ShopOffer o;
        o.id = items[size_t(i)];
        o.price = price(itemDef(ItemId(o.id)).price + rng.range(-3, 3));
        offers.push_back(o);
    }
    auto relics = availableRelics(run.player);
    rng.shuffle(relics);
    for (size_t i = 0; i < relics.size() && i < 2; ++i) {
        ShopOffer o;
        o.relic = true;
        o.id = int(relics[i]);
        o.price = price(relicDef(relics[i]).price + rng.range(-8, 8));
        offers.push_back(o);
    }
    return offers;
}

bool buy(Run& run, ShopOffer& offer) {
    Player& p = run.player;
    if (offer.sold || p.gold < offer.price) return false;
    if (offer.relic) {
        if (p.has(RelicId(offer.id))) return false;
        p.relics.push_back(RelicId(offer.id));
    } else if (!addItem(p, ItemId(offer.id))) {
        return false;
    }
    p.gold -= offer.price;
    offer.sold = true;
    return true;
}

void restHeal(Player& player) {
    const int amount = std::max(1, player.maxHp * balance::kRestHealPercent / 100);
    player.hp = std::min(player.maxHp, player.hp + amount);
    player.poison = 0;
}

void restFortify(Player& player) {
    player.maxHp += balance::kRestFortifyMaxHp;
    player.hp += balance::kRestFortifyMaxHp;
}

TreasureLoot takeTreasure(Run& run, int nodeId) {
    Rng rng(deriveSeed(run.seed, 0x7000 + uint64_t(run.act) * 100 + uint64_t(nodeId)));
    TreasureLoot loot;
    loot.gold = rng.range(8, 16);
    const auto pool = availableRelics(run.player);
    if (pool.empty()) {
        loot.gold += 30;
    } else {
        loot.hasRelic = true;
        loot.relic = rng.pick(pool);
        run.player.relics.push_back(loot.relic);
    }
    run.player.gold += loot.gold;
    run.player.goldEarned += loot.gold;
    return loot;
}

// ---- Events -----------------------------------------------------------------

namespace {

constexpr EventDef kEvents[] = {
    {"Blood Shrine", "A stone altar hums. A carved hand waits, palm up.",
     {"Offer 5 HP for a relic", "Leave"}},
    {"Dice in the Dark", "A grinning stranger rattles two bone dice. \"Double or nothing?\"",
     {"Bet 25 gold (50%: win 60)", "Leave"}},
    {"Glowing Spring", "Clear water bubbles up between the stones.",
     {"Drink (heal 50%, cure poison)", "Fill a flask (gain a Potion)"}},
    {"Old Cartographer", "\"I know these tunnels. For a price, I'll show you a safe way in.\"",
     {"Pay 15 gold: next board opens wider", "Leave"}},
    {"Cursed Chest", "Chains wrapped in purple thread. Something inside clinks.",
     {"Open it: 2 items, next board cursed", "Leave"}},
    {"Training Dummy", "A straw dummy, scarred by many blades.",
     {"Spar: lose 3 HP, gain 8 XP", "Leave"}},
};
static_assert(sizeof(kEvents) / sizeof(kEvents[0]) == int(EventId::Count));

} // namespace

const EventDef& eventDef(EventId id) { return kEvents[int(id)]; }

EventId eventAt(const Run& run, int nodeId) {
    return EventId(deriveSeed(run.seed, 0x9000 + uint64_t(run.act) * 100 + uint64_t(nodeId)) %
                   uint64_t(EventId::Count));
}

bool eventOptionAllowed(const Run& run, EventId id, int option) {
    const Player& p = run.player;
    if (option != 0) return id != EventId::Spring || int(p.items.size()) < p.maxItems;
    switch (id) {
        case EventId::BloodShrine: return p.hp > 5 && !availableRelics(p).empty();
        case EventId::Gamble: return p.gold >= 25;
        case EventId::Spring: return true;
        case EventId::Cartographer: return p.gold >= 15;
        case EventId::CursedChest: return true;
        case EventId::TrainingDummy: return p.hp > 3;
        case EventId::Count: break;
    }
    return false;
}

std::string resolveEvent(Run& run, int nodeId, int option) {
    const EventId id = eventAt(run, nodeId);
    if (!eventOptionAllowed(run, id, option)) return "";
    Player& p = run.player;
    Rng rng(deriveSeed(run.seed, 0x9100 + uint64_t(run.act) * 100 + uint64_t(nodeId)));
    std::vector<Event> ignored;
    switch (id) {
        case EventId::BloodShrine: {
            if (option == 1) return "You leave the altar untouched.";
            p.hp -= 5;
            const RelicId r = rng.pick(availableRelics(p));
            p.relics.push_back(r);
            return std::string("The altar drinks. You receive: ") + relicDef(r).name + ".";
        }
        case EventId::Gamble:
            if (option == 1) return "The stranger shrugs and vanishes.";
            p.gold -= 25;
            if (rng.chance(0.5)) {
                p.gold += 60;
                p.goldEarned += 60;
                return "Snake eyes... for him! You win 60 gold.";
            }
            return "The dice betray you. 25 gold gone.";
        case EventId::Spring:
            if (option == 0) {
                p.hp = std::min(p.maxHp, p.hp + p.maxHp / 2);
                p.poison = 0;
                return "Cool and bright. You feel restored.";
            }
            addItem(p, ItemId::Potion);
            return "You fill a flask. (+1 Potion)";
        case EventId::Cartographer:
            if (option == 1) return "\"Your loss,\" he mutters.";
            p.gold -= 15;
            run.nextBonusRadius = 2;
            return "He sketches a safe path. Your next board opens wider.";
        case EventId::CursedChest: {
            if (option == 1) return "Some things are best left shut.";
            std::string got;
            for (int i = 0; i < 2; ++i) {
                const ItemId item = ItemId(rng.range(0, kItemKinds - 1));
                if (addItem(p, item)) got += std::string(got.empty() ? "" : ", ") + itemDef(item).name;
            }
            run.nextHexed = true;
            return "Purple smoke curls around you. Found: " + (got.empty() ? std::string("nothing (bag full)") : got) +
                   ". Your next board is cursed.";
        }
        case EventId::TrainingDummy:
            if (option == 1) return "You save your strength.";
            p.hp -= 3;
            gainXp(p, 8, ignored);
            return "A good workout. (+8 XP)";
        case EventId::Count: break;
    }
    return "";
}

} // namespace flagd
