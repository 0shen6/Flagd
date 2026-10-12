#pragma once
// Player + one board ("encounter") and the rules that connect them. All game
// rules run through applyAction(), which returns Events for the UI to animate.
#include <cstdint>
#include <vector>

#include "core/board.h"
#include "core/content/balance.h"
#include "core/content/content.h"
#include "core/rng.h"

namespace flagd {

struct Player {
    ClassId cls = ClassId::Warrior;
    int hp = 10;
    int maxHp = 10;
    int level = 1;
    int xp = 0;
    int gold = 0;
    int shield = 0;
    std::vector<ItemId> items;
    std::vector<RelicId> relics;

    // Run statistics
    int kills = 0;
    int tilesRevealed = 0;
    int floorsCleared = 0;
    int goldEarned = 0;

    bool has(RelicId r) const;
    bool alive() const { return hp > 0; }
};

Player makePlayer(ClassId cls);

struct Encounter {
    balance::FloorParams params;
    uint64_t seed = 0;
    Board board;
    bool generated = false;
    int turn = 0;
    int revealClicks = 0;  // clicks on hidden cells (Tenth Step)
    bool hasKey = false;
    bool exitFound = false;
    bool cleared = false;
    bool won = false;        // boss slain
    bool lost = false;       // player died
    bool descended = false;  // took the exit
    int hintCount = 0;
    std::vector<int> flareRows;
    std::vector<int> flareCols;
    Rng rng;
};

// The board is generated on the first reveal so that click is always safe.
Encounter makeEncounter(const balance::FloorParams& params, uint64_t seed);

// Reveal on a revealed number chords it; on a closed chest it opens the chest.
enum class ActionType : uint8_t { Reveal, CycleMark, SetMark, UseItem, Descend };

struct Action {
    ActionType type = ActionType::Reveal;
    Pos pos{};
    Mark mark = Mark::None;  // SetMark
    int slot = -1;           // UseItem
};

enum class EventType : uint8_t {
    Damage,       // amount = HP lost, aux = 1 if it was an ambush
    Heal,         // amount = HP gained
    Gold,         // amount = gold gained
    Kill,         // aux = MonsterKind
    BossHit,      // amount = hits left
    BossDefeated,
    XpGained,     // amount
    LevelUp,      // amount = new level
    KeyFound,
    ChestOpened,
    NeedKey,
    ItemGained,   // aux = ItemId
    InventoryFull,
    RelicGained,  // aux = RelicId
    ItemUsed,     // aux = ItemId
    ExitFound,
    Descend,
    BoardCleared, // amount = bonus gold
    ShieldBlock,
    Warded,       // Tenth Step stopped a fight
    WrongFlag,
    Burrow,       // the Burrower re-covered pos
    HintsPlaced,  // amount = hint tiles revealed
    PlayerDied,
    Invalid,
};

struct Event {
    EventType type;
    Pos pos{};
    int amount = 0;
    int aux = 0;
};

std::vector<Event> applyAction(Player& player, Encounter& enc, const Action& action);

// Adds an item if there is room. Returns false if the inventory is full.
bool addItem(Player& player, ItemId id);
void gainXp(Player& player, int amount, std::vector<Event>& events);
void heal(Player& player, int amount, std::vector<Event>& events);

// Total power of living monsters with a cell in this row / column (Flare).
int rowThreat(const Board& b, int y);
int colThreat(const Board& b, int x);

// Reveals left before Tenth Step protects a click (1 = the next click).
int tenthStepCountdown(const Encounter& enc);

} // namespace flagd
