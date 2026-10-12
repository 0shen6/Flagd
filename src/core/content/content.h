#pragma once
// Static game content: monsters, items, relics, classes. Tables live in content.cpp.
#include <cstdint>

namespace flagd {

enum class MonsterKind : uint8_t { Slime, Bat, Skeleton, Spider, Burrower, Count };

struct MonsterDef {
    const char* name;
    int power;    // threat it adds to neighbouring numbers
    int hits;     // attacks needed to kill
    int hitCost;  // HP lost per deliberate attack
    int xp;       // XP per hit landed
    bool boss;
    const char* blurb;
};

enum class ItemId : uint8_t { Scanner, Defuser, Flare, Shield, Potion, Count };

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
    Count
};

struct RelicDef {
    const char* name;
    const char* desc;
    int price;
};

enum class ClassId : uint8_t { Warrior, Count };

struct ClassDef {
    const char* name;
    const char* desc;
    int maxHp;
    int xpPercent;  // XP gain multiplier, in percent
};

constexpr int kMonsterKinds = int(MonsterKind::Count);
constexpr int kItemKinds = int(ItemId::Count);
constexpr int kRelicKinds = int(RelicId::Count);

const MonsterDef& monsterDef(MonsterKind k);
const ItemDef& itemDef(ItemId id);
const RelicDef& relicDef(RelicId id);
const ClassDef& classDef(ClassId id);

} // namespace flagd
