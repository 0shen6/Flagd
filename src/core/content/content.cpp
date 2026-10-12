#include "core/content/content.h"

namespace flagd {

namespace {

constexpr MonsterDef kMonsters[] = {
    // name        pow hits cost xp boss  blurb
    {"Slime",       1,  1,  1,  1, false, "Weak and wobbly."},
    {"Bat",         2,  1,  2,  2, false, "Bites, then flutters off."},
    {"Skeleton",    3,  1,  3,  3, false, "Rattles in the dark."},
    {"Spider",      5,  1,  5,  5, false, "Patient. Venomous."},
    {"Burrower",    8,  3,  5,  4, true,  "Buries what you uncover."},
};
static_assert(sizeof(kMonsters) / sizeof(kMonsters[0]) == kMonsterKinds);

constexpr ItemDef kItems[] = {
    {"Scanner", "Expose a 3x3 area. Monsters are shown, not fought.", 22, true},
    {"Defuser", "Slay a monster without taking damage.", 26, true},
    {"Flare",   "Show the total threat in a row and column.", 14, true},
    {"Shield",  "The next fight costs no HP.", 18, false},
    {"Potion",  "Restore 5 HP.", 16, false},
};
static_assert(sizeof(kItems) / sizeof(kItems[0]) == kItemKinds);

constexpr RelicDef kRelics[] = {
    {"Lucky Clover",  "Your first click opens a larger safe area.", 70},
    {"Tenth Step",    "Every 10th reveal can't trigger a fight.", 85},
    {"Bounty Flags",  "Attacking a flagged monster: +3 gold. Wrong flag: -1 HP.", 75},
    {"Chord Miner",   "+1 gold for each chord that opens tiles.", 65},
    {"Wide Chord",    "Chords also open safe tiles one ring further.", 95},
    {"Thick Skin",    "Take 1 less damage from every fight.", 90},
    {"Coin Magnet",   "+50% gold from tiles.", 70},
};
static_assert(sizeof(kRelics) / sizeof(kRelics[0]) == kRelicKinds);

constexpr ClassDef kClasses[] = {
    {"Warrior", "Sturdy and battle-hungry: +2 max HP, +50% XP from fights.", 12, 150},
};
static_assert(sizeof(kClasses) / sizeof(kClasses[0]) == int(ClassId::Count));

} // namespace

const MonsterDef& monsterDef(MonsterKind k) { return kMonsters[int(k)]; }
const ItemDef& itemDef(ItemId id) { return kItems[int(id)]; }
const RelicDef& relicDef(RelicId id) { return kRelics[int(id)]; }
const ClassDef& classDef(ClassId id) { return kClasses[int(id)]; }

} // namespace flagd
