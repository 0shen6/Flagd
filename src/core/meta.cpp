#include "core/meta.h"

#include <sstream>

namespace flagd {

namespace {

constexpr UpgradeDef kUpgrades[] = {
    {"Vitality", "+1 starting max HP per level.", 3, {15, 30, 45}},
    {"Purse", "+10 starting gold per level.", 3, {10, 20, 30}},
    {"Prepared", "Start every run with an extra Potion.", 1, {25, 0, 0}},
    {"Cartography", "Start every run with a Flare.", 1, {20, 0, 0}},
};
static_assert(sizeof(kUpgrades) / sizeof(kUpgrades[0]) == kUpgradeKinds);

} // namespace

const UpgradeDef& upgradeDef(UpgradeId id) { return kUpgrades[int(id)]; }

bool classUnlocked(const Meta& m, ClassId c) {
    return classDef(c).unlockHint == nullptr || ((m.classes >> int(c)) & 1u);
}

uint32_t relicPool(const Meta& m) { return m.relics ? m.relics : baseRelicPool(); }

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
    return r;
}

RunResult recordRun(Meta& m, const Run& run) {
    RunResult res;
    const Player& p = run.player;
    int embers = (run.deepestLayer + 1) * 3 + p.kills / 4 + (run.victory ? 20 : 0);
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
    if (run.victory) m.bossesBeaten |= 1u << int(run.map.bossKind);

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
    m.relics = pool | (1u << int(id));
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
    o << '\n';
    return o.str();
}

bool loadMeta(const std::string& text, Meta& m) {
    std::istringstream in(text);
    std::string key;
    int version = 0;
    if (!(in >> key >> version) || key != "flagd_save") return false;
    Meta out;
    while (in >> key) {
        if (key == "embers") in >> out.embers;
        else if (key == "runs") in >> out.runs;
        else if (key == "wins") in >> out.wins;
        else if (key == "bestLayer") in >> out.bestLayer;
        else if (key == "maxHeat") in >> out.maxHeat;
        else if (key == "classes") in >> out.classes;
        else if (key == "relics") in >> out.relics;
        else if (key == "seenMonsters") in >> out.seenMonsters;
        else if (key == "seenRelics") in >> out.seenRelics;
        else if (key == "seenModifiers") in >> out.seenModifiers;
        else if (key == "bossesBeaten") in >> out.bossesBeaten;
        else if (key == "kills") for (int& k : out.kills) in >> k;
        else if (key == "upgrades") for (int& u : out.upgrades) in >> u;
        else return false;
        if (in.fail()) return false;
    }
    m = out;
    return true;
}

} // namespace flagd
