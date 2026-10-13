#pragma once
// A whole run: player, map position, and the non-board rooms (shop, rest, treasure, events).
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/game_state.h"
#include "core/map.h"

namespace flagd {

// A run is three acts (Slay the Spire style): each act is its own map that
// ends in a boss. Beating a boss grants a relic choice and opens the next act.
struct Run {
    uint64_t seed = 0;
    int heat = 0;
    int act = 1;                              // 1..kActs
    std::array<MonsterKind, kActs> bosses{};  // one boss per act, no repeats
    bool actCleared = false;                  // this act's boss is down; reward pending
    uint32_t bossesBeaten = 0;                // bitmask of MonsterKind
    Player player;
    Map map;
    int current = -1;       // node id; -1 before the first choice
    std::vector<int> path;  // nodes visited, in order
    bool over = false;
    bool victory = false;
    int deepestLayer = -1;  // deepest floor entered, counted across acts (0-based)

    // Effects carried into the next board (from events).
    int nextBonusRadius = 0;
    bool nextHexed = false;

    // Nodes the player may move to next.
    std::vector<int> choices() const;
    const MapNode& node(int id) const { return map.nodes[size_t(id)]; }
};

Run newRun(uint64_t seed, ClassId cls, int heat = 0);

// Moves to a node offered by choices(). Returns false if not allowed.
bool travel(Run& run, int nodeId);

// The board for a node, as rolled from the run seed.
Encounter makeEncounterFor(const Run& run, int nodeId);
// Same, and applies (then clears) effects carried over from events.
Encounter beginEncounter(Run& run, int nodeId);

// Call after a board ends (descended or boss slain).
void finishBoard(Run& run, const Encounter& enc);

// Floor number across the whole run (0-based) for a layer of the current act.
int floorIndex(const Run& run, int layer);

// Relics offered after an act's boss (up to 3, seeded).
std::vector<RelicId> bossRelicChoices(const Run& run);
// Moves to the next act: new map and boss, and a heal.
void advanceAct(Run& run);

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

// ---- Events ("?" nodes) -----------------------------------------------------

enum class EventId : uint8_t { BloodShrine, Gamble, Spring, Cartographer, CursedChest, TrainingDummy, Count };

struct EventDef {
    const char* title;
    const char* text;
    const char* options[2];
};

const EventDef& eventDef(EventId id);
EventId eventAt(const Run& run, int nodeId);
bool eventOptionAllowed(const Run& run, EventId id, int option);
// Applies the chosen option and returns a line describing what happened.
std::string resolveEvent(Run& run, int nodeId, int option);

} // namespace flagd
