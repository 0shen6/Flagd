#pragma once
// Static game content: monsters, items, relics, classes, modifiers, heat.
// Tables live in content.cpp.
#include <cstdint>

namespace flagd {

enum class MonsterKind : uint8_t {
    Slime,
    Bat,
    Skeleton,
    Spider,
    Keg,         // explosive
    Toad,        // poison (the Plague Polyp)
    Idol,        // curse
    Golem,       // dormant
    Wisp,        // wanders
    Burrower,    // boss: buries tiles
    FogWyrm,     // boss: fogged numbers
    HiveMother,  // boss: spawns wisps
    Count
};

// What happens beyond plain damage when a monster is fought.
enum class Trait : uint8_t {
    None,
    Explosive,  // ambush: blast damage and the 3x3 around it is exposed
    Poison,     // ambush: lose 1 HP per turn for a few turns
    Curse,      // ambush: XP halved for the rest of the board
    Dormant,    // free to fight once nothing around it is hidden
    Wanderer,   // visible; moves one tile per turn, lighting its trail
};

struct MonsterDef {
    const char* name;
    int power;         // threat it adds to neighbouring numbers
    int hits;          // attacks needed to kill
    int hitCost;       // HP lost per deliberate attack
    int xp;            // XP per hit landed
    bool boss;
    Trait trait;
    int ambushDamage;  // 0 = hitCost x ambush multiplier
    const char* blurb;
};

enum class ItemId : uint8_t { Scanner, Defuser, Flare, Shield, Potion, Bomb, Elixir, Compass, Whetstone, Count };

struct ItemDef {
    const char* name;
    const char* desc;
    int price;
    bool targeted;  // needs a board cell to use
};

enum class RelicId : uint8_t {
    LuckyStart,
    TenthStep,
    BountyFlags,
    ChordMiner,
    WideChord,
    ThickSkin,
    CoinMagnet,
    MisersPurse,
    ScholarsTome,
    HealingHerb,
    // Unlocked at the Camp:
    GasMask,
    HolyWater,
    BlastWard,
    VampireFang,
    Lantern,
    IronBoots,
    SecondWind,
    TreasureSense,
    ArmorBreaker,
    Count
};

struct RelicDef {
    const char* name;
    const char* desc;
    int price;
    int unlockCost;  // embers at the Camp; 0 = available from the start
};

enum class ClassId : uint8_t { Warrior, Sapper, Seer, Gambler, Count };

struct ClassDef {
    const char* name;
    const char* desc;
    int maxHp;
    int xpPercent;  // XP gain multiplier, in percent
    int startGold;
    const char* unlockHint;  // shown while locked; nullptr = always unlocked
};

// Board-wide rule twists, shown on the map before you commit.
enum class Modifier : uint8_t { None, Wrap, Hex, Fog, Liars, Count };

struct ModifierDef {
    const char* name;
    const char* desc;
};

constexpr int kMonsterKinds = int(MonsterKind::Count);
constexpr int kItemKinds = int(ItemId::Count);
constexpr int kRelicKinds = int(RelicId::Count);
constexpr int kClassKinds = int(ClassId::Count);
constexpr int kModifierKinds = int(Modifier::Count);
constexpr int kMaxHeat = 5;
constexpr int kActs = 3;
static_assert(kRelicKinds <= 32, "relic sets are 32-bit masks");

const MonsterDef& monsterDef(MonsterKind k);
const ItemDef& itemDef(ItemId id);
const RelicDef& relicDef(RelicId id);
const ClassDef& classDef(ClassId id);
const ModifierDef& modifierDef(Modifier m);
const char* heatDesc(int level);  // what heat `level` adds (1..kMaxHeat)
const char* traitDesc(Trait t);

} // namespace flagd
