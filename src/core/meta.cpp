#include "core/meta.h"

#include <sstream>

namespace flagd {

namespace {

constexpr UpgradeDef kUpgrades[] = {
    {"Vitality", "+1 starting max HP per level.", 3, {15, 30, 45}},
    {"Purse", "+10 starting gold per level.", 3, {10, 20, 30}},
    {"Prepared", "Start every run with an extra Potion.", 1, {25, 0, 0}},
    {"Cartography", "Start every run with a Flare.", 1, {20, 0, 0}},
    {"Sharp Eyes", "Start every run with a Scanner.", 1, {20, 0, 0}},
    {"Deep Pockets", "Carry 5 items instead of 4.", 1, {40, 0, 0}},
    {"Veteran", "Start every run at level 2.", 1, {35, 0, 0}},
    {"Haggler", "Shops charge 10% less per level.", 2, {15, 30, 0}},
};
static_assert(sizeof(kUpgrades) / sizeof(kUpgrades[0]) == kUpgradeKinds);

} // namespace

const UpgradeDef& upgradeDef(UpgradeId id) { return kUpgrades[int(id)]; }

bool classUnlocked(const Meta& m, ClassId c) {
    return classDef(c).unlockHint == nullptr || ((m.classes >> int(c)) & 1u);
}

uint32_t relicPool(const Meta& m) { return m.relics | baseRelicPool(); }

Run startRun(const Meta& m, uint64_t seed, ClassId cls, int heat) {
    Run r = newRun(seed, cls, heat);
    Player& p = r.player;
    p.relicPool = relicPool(m);
    const int vit = m.upgrades[int(UpgradeId::Vitality)];
    p.maxHp += vit;
    p.hp += vit;
    p.gold += 10 * m.upgrades[int(UpgradeId::Purse)];
    if (m.upgrades[int(UpgradeId::Prepared)] > 0) addItem(p, ItemId::Potion);
    if (m.upgrades[int(UpgradeId::Cartography)] > 0) addItem(p, ItemId::Flare);
    if (m.upgrades[int(UpgradeId::DeepPockets)] > 0) p.maxItems = balance::kInventorySlots + 1;
    if (m.upgrades[int(UpgradeId::SharpEyes)] > 0) addItem(p, ItemId::Scanner);
    if (m.upgrades[int(UpgradeId::Veteran)] > 0) {
        p.level = 2;
        p.maxHp += balance::kLevelUpMaxHp;
        p.hp += balance::kLevelUpMaxHp;
    }
    p.shopDiscount = 10 * m.upgrades[int(UpgradeId::Haggler)];
    return r;
}

RunResult recordRun(Meta& m, const Run& run) {
    RunResult res;
    const Player& p = run.player;
    int embers = (run.deepestLayer + 1) * 2 + p.kills / 4 + 8 * __builtin_popcount(run.bossesBeaten) +
                 (run.victory ? 20 : 0);
    embers = embers * (100 + 15 * run.heat) / 100;
    res.embers = embers;
    m.embers += embers;
    ++m.runs;
    m.wins += run.victory;
    m.bestLayer = std::max(m.bestLayer, run.deepestLayer);
    m.seenMonsters |= p.seenKinds;
    m.seenModifiers |= p.seenModifiers;
    for (RelicId r : p.relics) m.seenRelics |= 1u << int(r);
    for (int k = 0; k < kMonsterKinds; ++k) m.kills[size_t(k)] += p.killsByKind[size_t(k)];
    m.bossesBeaten |= run.bossesBeaten;

    auto unlockClass = [&](ClassId c, bool earned) {
        if (!earned || classUnlocked(m, c)) return;
        m.classes |= 1u << int(c);
        res.unlocks.push_back(std::string("New class: ") + classDef(c).name);
    };
    unlockClass(ClassId::Sapper, run.deepestLayer >= 3);
    unlockClass(ClassId::Seer, run.victory);
    unlockClass(ClassId::Gambler, p.goldEarned >= 150);
    if (run.victory && run.heat == m.maxHeat && m.maxHeat < kMaxHeat) {
        ++m.maxHeat;
        res.unlocks.push_back("Heat " + std::to_string(m.maxHeat) + " unlocked: " + heatDesc(m.maxHeat));
    }
    return res;
}

int upgradeCost(const Meta& m, UpgradeId id) {
    const UpgradeDef& d = upgradeDef(id);
    const int lvl = m.upgrades[int(id)];
    return lvl >= d.maxLevel ? -1 : d.costs[lvl];
}

bool buyUpgrade(Meta& m, UpgradeId id) {
    const int cost = upgradeCost(m, id);
    if (cost < 0 || m.embers < cost) return false;
    m.embers -= cost;
    ++m.upgrades[int(id)];
    return true;
}

bool buyRelicUnlock(Meta& m, RelicId id) {
    const uint32_t pool = relicPool(m);
    const int cost = relicDef(id).unlockCost;
    if ((pool >> int(id)) & 1u || m.embers < cost) return false;
    m.embers -= cost;
    m.relics |= 1u << int(id);
    return true;
}

std::string saveMeta(const Meta& m) {
    std::ostringstream o;
    o << "flagd_save 1\n";
    o << "embers " << m.embers << "\nruns " << m.runs << "\nwins " << m.wins << "\nbestLayer " << m.bestLayer
      << "\nmaxHeat " << m.maxHeat << "\nclasses " << m.classes << "\nrelics " << m.relics << "\nseenMonsters "
      << m.seenMonsters << "\nseenRelics " << m.seenRelics << "\nseenModifiers " << m.seenModifiers
      << "\nbossesBeaten " << m.bossesBeaten << "\nkills";
    for (int k : m.kills) o << ' ' << k;
    o << "\nupgrades";
    for (int u : m.upgrades) o << ' ' << u;
    o << "\nfullscreen " << m.fullscreen << "\nmuted " << m.muted << "\npixelPerfect " << m.pixelPerfect
      << "\nwindowPreset " << m.windowPreset << '\n';
    return o.str();
}

// Line-based so saves from older versions (fewer upgrades or monsters) load:
// missing values keep their defaults and unknown keys are skipped.
bool loadMeta(const std::string& text, Meta& m) {
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line.rfind("flagd_save", 0) != 0) return false;
    Meta out;
    auto readArray = [](std::istringstream& ls, auto& arr) {
        for (auto& v : arr)
            if (!(ls >> v)) break;
    };
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string key;
        if (!(ls >> key)) continue;
        if (key == "embers") ls >> out.embers;
        else if (key == "runs") ls >> out.runs;
        else if (key == "wins") ls >> out.wins;
        else if (key == "bestLayer") ls >> out.bestLayer;
        else if (key == "maxHeat") ls >> out.maxHeat;
        else if (key == "classes") ls >> out.classes;
        else if (key == "relics") ls >> out.relics;
        else if (key == "seenMonsters") ls >> out.seenMonsters;
        else if (key == "seenRelics") ls >> out.seenRelics;
        else if (key == "seenModifiers") ls >> out.seenModifiers;
        else if (key == "bossesBeaten") ls >> out.bossesBeaten;
        else if (key == "kills") readArray(ls, out.kills);
        else if (key == "upgrades") readArray(ls, out.upgrades);
        else if (key == "fullscreen") ls >> out.fullscreen;
        else if (key == "muted") ls >> out.muted;
        else if (key == "pixelPerfect") ls >> out.pixelPerfect;
        else if (key == "windowPreset") ls >> out.windowPreset;
    }
    m = out;
    return true;
}

} // namespace flagd
