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

inline int xpToNext(int level) { return 4 + 3 * level; }

// Chance a safe tile holds gold rises with its threat: risky tiles pay more.
inline double goldChance(int threat) { return std::min(0.6, 0.05 + 0.05 * threat); }
inline int goldAmount(int threat, int roll) { return 1 + threat / 2 + roll; }
inline int clearBonus(int depth) { return 8 + 3 * depth; }
inline int shopPrice(int base, int heat) { return heat >= 2 ? base + base / 4 : base; }

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
};

inline MonsterKind bossKindFor(int index) {
    constexpr MonsterKind kBosses[] = {MonsterKind::Burrower, MonsterKind::FogWyrm, MonsterKind::HiveMother};
    return kBosses[((index % 3) + 3) % 3];
}

inline FloorParams floorParams(int depth, bool elite, bool boss, MonsterKind bossKind = MonsterKind::Burrower,
                               Modifier modifier = Modifier::None, int heat = 0) {
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
    if (boss) {
        p.width = 14;
        p.height = 9;
        p.counts = {6, 5, 3, 2, 1, 1, 0, 0, 0};
        if (bossKind == K::HiveMother) p.counts = {5, 4, 2, 1, 1, 1, 0, 0, 0};
        p.counts[int(bossKind)] = 1;
        p.fog = bossKind == K::FogWyrm;
        p.bossExtraHits = heat >= 5 ? 2 : 0;
    } else {
        const Row& r = kRows[std::clamp(depth, 0, kRowCount - 1)];
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
    if (heat >= 1) {
        for (int k = 0; k < kMonsterKinds; ++k)
            if (!monsterDef(MonsterKind(k)).boss && p.counts[size_t(k)] > 0)
                p.counts[size_t(k)] += (p.counts[size_t(k)] * 15 + 50) / 100;  // +15%, rounded
    }
    return p;
}

} // namespace flagd::balance
