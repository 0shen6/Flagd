#include "core/content/content.h"

namespace flagd {

namespace {

constexpr MonsterDef kMonsters[] = {
    // name           pow hits cost xp boss  trait               ambush blurb
    {"Slime",          1,  1,  1,  1, false, Trait::None,        0, "Weak and wobbly."},
    {"Bat",            2,  1,  2,  2, false, Trait::None,        0, "Bites, then flutters off."},
    {"Skeleton",       3,  1,  3,  3, false, Trait::None,        0, "Rattles in the dark."},
    {"Spider",         5,  1,  5,  5, false, Trait::None,        0, "Patient. Venomous."},
    {"Powder Keg",     2,  1,  1,  1, false, Trait::Explosive,   4, "Defuse it gently, or it blows open everything around it."},
    {"Plague Polyp",   1,  1,  1,  1, false, Trait::Poison,      0, "Its surprise sting poisons you."},
    {"Cursed Idol",    2,  1,  2,  2, false, Trait::Curse,       0, "Stumble into it and XP is halved for the board."},
    {"Sleeping Golem", 3,  1,  3,  4, false, Trait::Dormant,     0, "Uncover everything around it and it never wakes: free XP."},
    {"Wisp",           2,  1,  2,  2, false, Trait::Wanderer,    0, "Drifts one tile per turn, lighting its trail. Numbers shift with it."},
    {"Burrower",       8,  3,  4,  4, true,  Trait::None,        0, "Buries a revealed number every few turns."},
    {"Fog Wyrm",       8,  3,  5,  4, true,  Trait::None,        0, "Its breath fogs every number. Each hit clears the air around it."},
    {"Hive Mother",    6,  3,  4,  4, true,  Trait::None,        0, "Births a wandering Wisp every few turns."},
};
static_assert(sizeof(kMonsters) / sizeof(kMonsters[0]) == kMonsterKinds);

constexpr ItemDef kItems[] = {
    {"Scanner", "Expose a 3x3 area. Monsters are shown, not fought.", 22, true},
    {"Defuser", "Slay a monster without taking damage.", 26, true},
    {"Flare",   "Show the total threat in a row and column.", 14, true},
    {"Shield",  "The next fight costs no HP.", 18, false},
    {"Potion",  "Restore 5 HP and cure poison.", 16, false},
};
static_assert(sizeof(kItems) / sizeof(kItems[0]) == kItemKinds);

constexpr RelicDef kRelics[] = {
    {"Lucky Clover",  "Your first click opens a larger safe area.", 70, 0},
    {"Tenth Step",    "Every 10th reveal can't trigger a fight.", 85, 0},
    {"Bounty Flags",  "Attacking a flagged monster: +3 gold. Wrong flag: -1 HP.", 75, 0},
    {"Chord Miner",   "+1 gold for each chord that opens tiles.", 65, 0},
    {"Wide Chord",    "Chords also open safe tiles one ring further.", 95, 0},
    {"Thick Skin",    "Take 1 less damage from every fight.", 90, 0},
    {"Coin Magnet",   "+50% gold from tiles.", 70, 0},
    {"Gas Mask",      "Immune to poison.", 60, 20},
    {"Holy Water",    "Immune to curses.", 60, 20},
    {"Blast Ward",    "Explosions deal no damage.", 70, 25},
    {"Vampire Fang",  "Heal 1 HP whenever you slay a monster of power 3+.", 90, 35},
    {"Lantern",       "Fog never hides numbers.", 80, 30},
};
static_assert(sizeof(kRelics) / sizeof(kRelics[0]) == kRelicKinds);

constexpr ClassDef kClasses[] = {
    {"Warrior", "Sturdy and battle-hungry: +25% XP from fights.", 12, 125, 0, nullptr},
    {"Sapper",  "Starts with 2 Defusers. Explosions hurt half as much; kegs defuse for free.", 11, 110, 0,
     "Reach floor 4 to unlock."},
    {"Seer",    "Sees which kinds of monster lurk around each number. Starts with a Scanner. Frail.", 10, 130, 0,
     "Win a run to unlock."},
    {"Gambler", "Risky reveals that pay off heal 1 HP; ambushes cost triple. Starts with 40 gold.", 11, 115, 40,
     "Earn 150 gold in one run to unlock."},
};
static_assert(sizeof(kClasses) / sizeof(kClasses[0]) == kClassKinds);

constexpr ModifierDef kModifiers[] = {
    {"Plain", "No twist."},
    {"Wrap-around", "Edges connect: the left column touches the right, the top row touches the bottom."},
    {"Hex Grid", "Tiles are hexagons with six neighbours."},
    {"Fog", "Numbers show only a range, like 4-6."},
    {"Liars", "Some numbers (marked in purple) lie by 1 or 2."},
};
static_assert(sizeof(kModifiers) / sizeof(kModifiers[0]) == kModifierKinds);

constexpr const char* kHeat[] = {
    "No heat.",
    "Boards hold more monsters.",
    "Shops charge 25% more.",
    "Level-ups heal only half your max HP.",
    "Every board has a modifier.",
    "The boss takes 2 extra hits.",
};
static_assert(sizeof(kHeat) / sizeof(kHeat[0]) == kMaxHeat + 1);

} // namespace

const MonsterDef& monsterDef(MonsterKind k) { return kMonsters[int(k)]; }
const ItemDef& itemDef(ItemId id) { return kItems[int(id)]; }
const RelicDef& relicDef(RelicId id) { return kRelics[int(id)]; }
const ClassDef& classDef(ClassId id) { return kClasses[int(id)]; }
const ModifierDef& modifierDef(Modifier m) { return kModifiers[int(m)]; }
const char* heatDesc(int level) { return kHeat[level < 0 ? 0 : level > kMaxHeat ? kMaxHeat : level]; }

const char* traitDesc(Trait t) {
    switch (t) {
        case Trait::None: return "";
        case Trait::Explosive: return "Explosive";
        case Trait::Poison: return "Poison";
        case Trait::Curse: return "Curse";
        case Trait::Dormant: return "Dormant";
        case Trait::Wanderer: return "Wanders";
    }
    return "";
}

} // namespace flagd
