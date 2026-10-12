#include "core/run.h"

#include <algorithm>

namespace flagd {

namespace {

std::vector<RelicId> unownedRelics(const Player& p) {
    std::vector<RelicId> pool;
    for (int r = 0; r < kRelicKinds; ++r)
        if (!p.has(RelicId(r))) pool.push_back(RelicId(r));
    return pool;
}

} // namespace

std::vector<int> Run::choices() const {
    if (over) return {};
    if (current < 0) return map.layer(0);
    return map.nodes[size_t(current)].next;
}

Run newRun(uint64_t seed, ClassId cls) {
    Run r;
    r.seed = seed;
    r.player = makePlayer(cls);
    r.map = generateMap(seed);
    return r;
}

bool travel(Run& run, int nodeId) {
    const auto opts = run.choices();
    if (std::find(opts.begin(), opts.end(), nodeId) == opts.end()) return false;
    run.current = nodeId;
    run.path.push_back(nodeId);
    return true;
}

Encounter makeEncounterFor(const Run& run, int nodeId) {
    const MapNode& n = run.node(nodeId);
    const auto params = balance::floorParams(n.layer, n.type == NodeType::Elite, n.type == NodeType::Boss);
    return makeEncounter(params, deriveSeed(run.seed, uint64_t(nodeId) + 1));
}

void finishBoard(Run& run, const Encounter& enc) {
    if (enc.lost) {
        run.over = true;
        return;
    }
    ++run.player.floorsCleared;
    if (enc.won && enc.params.boss) {
        run.over = true;
        run.victory = true;
    }
}

std::vector<ShopOffer> makeShop(const Run& run, int nodeId) {
    Rng rng(deriveSeed(run.seed, 0x5000 + uint64_t(nodeId)));
    std::vector<ShopOffer> offers;
    std::vector<int> items;
    for (int i = 0; i < kItemKinds; ++i) items.push_back(i);
    rng.shuffle(items);
    for (int i = 0; i < 3; ++i) {
        ShopOffer o;
        o.id = items[size_t(i)];
        o.price = itemDef(ItemId(o.id)).price + rng.range(-3, 3);
        offers.push_back(o);
    }
    auto relics = unownedRelics(run.player);
    rng.shuffle(relics);
    for (size_t i = 0; i < relics.size() && i < 2; ++i) {
        ShopOffer o;
        o.relic = true;
        o.id = int(relics[i]);
        o.price = relicDef(relics[i]).price + rng.range(-8, 8);
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
}

void restFortify(Player& player) {
    player.maxHp += balance::kRestFortifyMaxHp;
    player.hp += balance::kRestFortifyMaxHp;
}

TreasureLoot takeTreasure(Run& run, int nodeId) {
    Rng rng(deriveSeed(run.seed, 0x7000 + uint64_t(nodeId)));
    TreasureLoot loot;
    loot.gold = rng.range(12, 24);
    auto pool = unownedRelics(run.player);
    if (pool.empty()) {
        loot.gold += 50;
    } else {
        loot.hasRelic = true;
        loot.relic = rng.pick(pool);
        run.player.relics.push_back(loot.relic);
    }
    run.player.gold += loot.gold;
    run.player.goldEarned += loot.gold;
    return loot;
}

} // namespace flagd
