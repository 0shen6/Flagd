#pragma once
// Tunable numbers in one place.
#include <algorithm>
#include <array>

#include "core/content/content.h"

namespace flagd::balance {

constexpr int kAmbushMultiplier = 2;   // fighting an unmarked hidden monster
constexpr int kLevelUpMaxHp = 2;
constexpr int kInventorySlots = 4;
constexpr int kPotionHeal = 5;
constexpr int kBountyGold = 3;
constexpr int kTenthStepEvery = 10;
constexpr int kBossBurrowEvery = 4;    // turns between Burrower re-covering a tile
constexpr int kGenAttempts = 16;       // boards tried before falling back to hint tiles
constexpr int kMapLayers = 8;          // last layer is the boss
constexpr int kRestHealPercent = 40;
constexpr int kRestFortifyMaxHp = 3;

inline int xpToNext(int level) { return 5 + 4 * level; }

// Chance a safe tile holds gold rises with its threat: risky tiles pay more.
inline double goldChance(int threat) { return std::min(0.6, 0.05 + 0.05 * threat); }
inline int goldAmount(int threat, int roll) { return 1 + threat / 2 + roll; }
inline int clearBonus(int depth) { return 8 + 3 * depth; }

struct FloorParams {
    int width = 9;
    int height = 9;
    std::array<int, kMonsterKinds> counts{};  // per MonsterKind
    bool boss = false;
    bool elite = false;
    int depth = 0;
};

inline FloorParams floorParams(int depth, bool elite, bool boss) {
    struct Row { int w, h, slime, bat, skel, spider; };
    // Boards up to 14x9 render at 2x; later floors trade size for density.
    static constexpr Row kRows[] = {
        { 9,  8,  5,  3, 1, 0},
        {11,  9,  7,  4, 2, 0},
        {13,  9,  8,  5, 3, 1},
        {14,  9,  8,  5, 4, 2},
        {16, 11, 11,  7, 5, 3},
        {18, 12, 13,  8, 7, 4},
        {20, 13, 15, 10, 8, 5},
    };
    constexpr int kRowCount = int(sizeof(kRows) / sizeof(kRows[0]));
    FloorParams p;
    p.depth = depth;
    p.elite = elite;
    p.boss = boss;
    if (boss) {
        p.width = 14;
        p.height = 9;
        p.counts = {6, 5, 3, 2, 1};
        return p;
    }
    const Row& r = kRows[std::clamp(depth, 0, kRowCount - 1)];
    p.width = r.w;
    p.height = r.h;
    p.counts = {r.slime, r.bat, r.skel, r.spider, 0};
    if (elite) {
        for (int i = 0; i < 4; ++i) p.counts[i] += (p.counts[i] + 3) / 4;
        p.counts[int(MonsterKind::Spider)] += 1;
    }
    return p;
}

} // namespace flagd::balance
