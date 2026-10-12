#pragma once
// Progress kept between runs: embers (currency), unlocks, a small upgrade
// tree, and codex discoveries. Serialized as plain "key value" text.
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/run.h"

namespace flagd {

enum class UpgradeId : uint8_t { Vitality, Purse, Prepared, Cartography, Count };
constexpr int kUpgradeKinds = int(UpgradeId::Count);

struct UpgradeDef {
    const char* name;
    const char* desc;
    int maxLevel;
    int costs[3];
};
const UpgradeDef& upgradeDef(UpgradeId id);

struct Meta {
    int embers = 0;
    int runs = 0;
    int wins = 0;
    int bestLayer = -1;
    int maxHeat = 0;  // highest heat unlocked
    uint32_t classes = 1u << int(ClassId::Warrior);
    uint32_t relics = 0;  // unlocked relic pool; 0 means "not set yet" (filled with the base pool)
    uint32_t seenMonsters = 0;
    uint32_t seenRelics = 0;
    uint32_t seenModifiers = 0;
    uint32_t bossesBeaten = 0;  // bitmask of MonsterKind
    std::array<int, kMonsterKinds> kills{};
    std::array<int, kUpgradeKinds> upgrades{};
};

bool classUnlocked(const Meta& m, ClassId c);
uint32_t relicPool(const Meta& m);

// A new run with the camp upgrades and unlocked relic pool applied.
Run startRun(const Meta& m, uint64_t seed, ClassId cls, int heat);

struct RunResult {
    int embers = 0;
    std::vector<std::string> unlocks;  // human-readable lines
};
// Banks a finished run: embers, codex, unlocks. Call once per run.
RunResult recordRun(Meta& m, const Run& run);

int upgradeCost(const Meta& m, UpgradeId id);  // -1 when maxed
bool buyUpgrade(Meta& m, UpgradeId id);
bool buyRelicUnlock(Meta& m, RelicId id);

std::string saveMeta(const Meta& m);
bool loadMeta(const std::string& text, Meta& m);

} // namespace flagd
