#pragma once
// A whole run: player, map position, and the non-board rooms (shop, rest, treasure).
#include <cstdint>
#include <vector>

#include "core/game_state.h"
#include "core/map.h"

namespace flagd {

struct Run {
    uint64_t seed = 0;
    Player player;
    Map map;
    int current = -1;       // node id; -1 before the first choice
    std::vector<int> path;  // nodes visited, in order
    bool over = false;
    bool victory = false;

    // Nodes the player may move to next.
    std::vector<int> choices() const;
    const MapNode& node(int id) const { return map.nodes[size_t(id)]; }
};

Run newRun(uint64_t seed, ClassId cls);

// Moves to a node offered by choices(). Returns false if not allowed.
bool travel(Run& run, int nodeId);

Encounter makeEncounterFor(const Run& run, int nodeId);

// Call after a board ends (descended or boss slain).
void finishBoard(Run& run, const Encounter& enc);

struct ShopOffer {
    bool relic = false;
    int id = 0;  // ItemId or RelicId
    int price = 0;
    bool sold = false;
};

std::vector<ShopOffer> makeShop(const Run& run, int nodeId);
bool buy(Run& run, ShopOffer& offer);

void restHeal(Player& player);
void restFortify(Player& player);

struct TreasureLoot {
    bool hasRelic = false;
    RelicId relic{};
    int gold = 0;
};
TreasureLoot takeTreasure(Run& run, int nodeId);

} // namespace flagd
