#pragma once
// Tunable numbers in one place.
#include <algorithm>
#include <array>

#include "core/content/content.h"

namespace flagd::balance {

constexpr int kAmbushMultiplier = 2;   // fighting an unmarked hidden monster
constexpr int kGamblerAmbushMultiplier = 3;
constexpr int kLevelUpMaxHp = 2;
constexpr int kInventorySlots = 4;
constexpr int kPotionHeal = 5;
constexpr int kBountyGold = 3;
constexpr int kTenthStepEvery = 10;
constexpr int kBossBurrowEvery = 4;    // turns between Burrower re-covering a tile
constexpr int kHiveSpawnEvery = 5;     // turns between Hive Mother births
constexpr int kHiveMaxWisps = 4;
constexpr int kPoisonTurns = 3;        // 1 HP per turn
constexpr int kGenAttempts = 16;       // boards tried before falling back to hint tiles
constexpr int kMapLayers = 8;          // last layer is the boss
constexpr int kRestHealPercent = 40;
constexpr int kRestFortifyMaxHp = 3;
constexpr int kWhetstoneCharges = 3;
constexpr int kElixirMaxHp = 2;
constexpr int kHerbHeal = 3;
constexpr int kBossArmor = 3;
// Boss armor breaks as the board is uncovered: one point at each fraction.
constexpr int kArmorBreaks[] = {40, 70, 95};  // percent of safe tiles revealed
constexpr int kActHealPercent = 100;           // heal between acts (half at heat 3+)

inline int xpToNext(int level) { return 4 + 3 * level; }

// Chance a safe tile holds gold rises with its threat: risky tiles pay more.
inline double goldChance(int threat) { return std::min(0.2, 0.015 + 0.015 * threat); }
inline int goldAmount(int threat, int roll) { return 1 + threat / 3 + roll; }
inline int clearBonus(int depth) { return 3 + depth; }
inline int shopPrice(int base, int heat, int act = 1) {
    const int p = base * (140 + 20 * (act - 1)) / 100;  // prices climb each act
    return heat >= 2 ? p + p / 4 : p;
}

struct FloorParams {
    int width = 9;
    int height = 9;
    std::array<int, kMonsterKinds> counts{};  // per MonsterKind
    bool boss = false;
    bool elite = false;
    int depth = 0;
    Modifier modifier = Modifier::None;
    bool fog = false;      // numbers shown as ranges
    int liars = 0;         // lying number tiles
    int bossExtraHits = 0;
    int act = 1;
};

inline MonsterKind bossKindFor(int index) {
    constexpr MonsterKind kBosses[] = {MonsterKind::Burrower, MonsterKind::FogWyrm, MonsterKind::HiveMother};
    return kBosses[((index % 3) + 3) % 3];
}

// `depth` is the map layer (0-based). Later acts start deeper in the table
// and pack in more monsters.
inline FloorParams floorParams(int depth, bool elite, bool boss, MonsterKind bossKind = MonsterKind::Burrower,
                               Modifier modifier = Modifier::None, int heat = 0, int act = 1) {
    using K = MonsterKind;
    struct Row { int w, h; std::array<int, 9> c; };  // slime bat skel spider keg toad idol golem wisp
    // Boards up to 14x9 render at 2x; later floors trade size for density.
    static constexpr Row kRows[] = {
        { 9,  8, {5, 3, 1, 0, 0, 0, 0, 0, 0}},
        {11,  9, {6, 3, 2, 0, 1, 1, 0, 0, 0}},
        {13,  9, {6, 4, 2, 1, 1, 1, 1, 0, 1}},
        {14,  9, {6, 4, 3, 1, 1, 1, 1, 1, 1}},
        {16, 11, {8, 5, 4, 2, 2, 1, 1, 1, 1}},
        {18, 12, {9, 6, 5, 3, 2, 2, 1, 2, 2}},
        {20, 13, {10, 7, 6, 4, 3, 2, 2, 2, 2}},
    };
    constexpr int kRowCount = int(sizeof(kRows) / sizeof(kRows[0]));
    FloorParams p;
    p.depth = depth;
    p.elite = elite;
    p.boss = boss;
    p.act = act;
    const int row = depth + 2 * (act - 1);
    if (boss) {
        // Boss arenas grow each act; the minion mix (tuned for 14x9) scales with area.
        static constexpr int kArena[3][2] = {{16, 10}, {18, 11}, {20, 12}};
        const auto& arena = kArena[std::clamp(act, 1, 3) - 1];
        p.width = arena[0];
        p.height = arena[1];
        p.counts = {6, 5, 3, 2, 1, 1, 0, 0, 0};
        if (bossKind == K::HiveMother) p.counts = {5, 4, 2, 1, 1, 1, 0, 0, 0};
        const int area = p.width * p.height;
        for (int k = 0; k < 9; ++k) p.counts[size_t(k)] = (p.counts[size_t(k)] * area + 63) / 126;
        p.counts[int(bossKind)] = 1;
        p.fog = bossKind == K::FogWyrm;
        p.bossExtraHits = (act - 1) + (heat >= 5 ? 2 : 0);
    } else {
        const Row& r = kRows[std::clamp(row, 0, kRowCount - 1)];
        p.width = r.w;
        p.height = r.h;
        for (int i = 0; i < 9; ++i) p.counts[size_t(i)] = r.c[size_t(i)];
        if (elite) {
            for (int i = 0; i < 4; ++i) p.counts[size_t(i)] += (p.counts[size_t(i)] + 3) / 4;
            p.counts[int(K::Spider)] += 1;
        }
        p.modifier = modifier;
        p.fog = modifier == Modifier::Fog;
        if (modifier == Modifier::Liars) p.liars = std::max(3, p.width * p.height / 16);
    }
    // Heat 1+ adds 15%; each act past the first adds 10% (rounded per kind).
    const int extra = (heat >= 1 ? 15 : 0) + 10 * (act - 1);
    if (extra > 0) {
        for (int k = 0; k < kMonsterKinds; ++k)
            if (!monsterDef(MonsterKind(k)).boss && p.counts[size_t(k)] > 0)
                p.counts[size_t(k)] += (p.counts[size_t(k)] * extra + 50) / 100;
    }
    return p;
}

} // namespace flagd::balance
