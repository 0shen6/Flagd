#pragma once
// Player + one board ("encounter") and the rules that connect them. All game
// rules run through applyAction(), which returns Events for the UI to animate.
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/board.h"
#include "core/content/balance.h"
#include "core/content/content.h"
#include "core/rng.h"

namespace flagd {

// Relics available from the start (unlockCost == 0), as a bitmask.
uint32_t baseRelicPool();

struct Player {
    ClassId cls = ClassId::Warrior;
    int hp = 10;
    int maxHp = 10;
    int level = 1;
    int xp = 0;
    int gold = 0;
    int shield = 0;
    int whetstone = 0;            // attacks left that cost 1 less HP
    int maxItems = balance::kInventorySlots;
    int shopDiscount = 0;         // percent off shop prices
    int poison = 0;               // turns of 1-damage poison left
    bool halfLevelHeal = false;   // heat 3+
    uint32_t relicPool = 0;       // relics that may drop this run (bitmask of RelicId)
    std::vector<ItemId> items;
    std::vector<RelicId> relics;

    // Run statistics (also feed the codex)
    int kills = 0;
    int tilesRevealed = 0;
    int floorsCleared = 0;
    int goldEarned = 0;
    std::array<int, kMonsterKinds> killsByKind{};
    uint32_t seenKinds = 0;      // bitmask of MonsterKind met
    uint32_t seenModifiers = 0;  // bitmask of Modifier met

    bool has(RelicId r) const;
    bool alive() const { return hp > 0; }
};

Player makePlayer(ClassId cls);

// Relics not owned and allowed by the run's pool.
std::vector<RelicId> availableRelics(const Player& p);

struct Encounter {
    balance::FloorParams params;
    uint64_t seed = 0;
    Board board;
    bool generated = false;
    int turn = 0;
    int revealClicks = 0;  // clicks on hidden cells (Tenth Step)
    int bonusRadius = 0;   // extra opening size (Cartographer event)
    bool hasKey = false;
    bool exitFound = false;
    bool cleared = false;
    bool hexed = false;      // cursed: XP halved for this board
    bool secondWindUsed = false;
    int bossArmor = 0;       // boss can't be hurt while > 0
    int bossArmorBase = 0;
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
    Damage,       // amount = HP lost, aux = 1 ambush, 2 ambush on an unproven mark
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
    Explosion,    // a keg blew at pos
    Poisoned,     // amount = turns
    PoisonTick,   // amount = damage
    Cursed,       // XP halved for this board
    Asleep,       // a dormant monster was slain without waking
    Lucky,        // Gambler: an unproven reveal paid off
    FogLifts,     // the Fog Wyrm was hit; fog cleared around it
    Spawned,      // a Wisp was born at pos
    BossArmored,  // attack bounced off; amount = armor, aux = % uncovered
    ArmorBroken,  // amount = armor left
    SecondWind,   // a killing blow left the player at 1 HP
    Sensed,       // Compass / Treasure Sense revealed feature locations
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
// Items that work off the board (on the map, in rooms): Potion, Elixir,
// Shield, Whetstone. The rest need a board to target or reveal.
bool usableAnywhere(ItemId id);
// Uses such an item outside a board. Returns a line describing the effect,
// or "" if it can't be used now (wrong item, or a Potion at full health).
std::string useItemOutside(Player& player, int slot);
void discardItem(Player& player, int slot);
void gainXp(Player& player, int amount, std::vector<Event>& events, bool halved = false);
void heal(Player& player, int amount, std::vector<Event>& events);

// Total power of living monsters with a cell in this row / column (Flare).
int rowThreat(const Board& b, int y);
int colThreat(const Board& b, int x);

// Reveals left before Tenth Step protects a click (1 = the next click).
int tenthStepCountdown(const Encounter& enc);

// Seer's sight: bitmask of MonsterKinds among living hidden neighbours of p.
uint32_t lurkingKinds(const Board& b, Pos p);

// Percent of the board's safe tiles that are revealed (boss armor progress).
int uncoveredPercent(const Board& b);
// The uncovered percent at which the next armor point breaks (0 if none).
int nextArmorBreak(const Encounter& enc);

} // namespace flagd
