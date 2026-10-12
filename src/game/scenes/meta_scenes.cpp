// Camp (permanent upgrades, relic unlocks) and the Codex.
#include "game/render/atlas.h"
#include "game/render/ui.h"
#include "game/scenes/scenes.h"

namespace flagd {

// ---- Camp -------------------------------------------------------------------

void CampScene::update(App& app, float) {
    if (IsKeyPressed(KEY_ESCAPE)) app.toTitle();
}

void CampScene::draw(App& app) {
    Meta& m = app.meta;
    ui::text("Camp", 20, 12, pal::Gold, 20);
    ui::text("Spend embers from past runs. Upgrades are small on purpose: skill stays central.", 20, 38, pal::Dim);
    atlas().draw(spr::Ember, 540, 10, pal::Orange);
    ui::text(std::to_string(m.embers), 560, 14, pal::Orange, 20);

    ui::text("UPGRADES", 20, 60, pal::Bone);
    for (int i = 0; i < kUpgradeKinds; ++i) {
        const UpgradeId id = UpgradeId(i);
        const UpgradeDef& d = upgradeDef(id);
        const Rectangle row{20, 74 + float(i) * 34, 600, 30};
        ui::panel(row);
        ui::text(d.name, row.x + 8, row.y + 4, pal::Bone);
        ui::text(d.desc, row.x + 8, row.y + 16, pal::Dim);
        for (int l = 0; l < d.maxLevel; ++l)
            DrawRectangle(int(row.x + 330 + float(l) * 12), int(row.y + 10), 9, 9,
                          l < m.upgrades[size_t(i)] ? pal::Orange : pal::Floor);
        const int cost = upgradeCost(m, id);
        const std::string label = cost < 0 ? "Maxed" : "Buy  " + std::to_string(cost);
        if (ui::button({row.x + 480, row.y + 5, 110, 20}, label, cost >= 0 && m.embers >= cost)) {
            if (buyUpgrade(m, id)) {
                note_ = std::string(d.name) + " improved.";
                app.saveProgress();
            }
        }
    }

    ui::text("RELIC UNLOCKS  (adds them to the pool that shops, chests and treasure draw from)", 20, 216, pal::Bone);
    int col = 0;
    for (int r = 0; r < kRelicKinds; ++r) {
        const RelicDef& d = relicDef(RelicId(r));
        if (d.unlockCost == 0) continue;
        const bool owned = (relicPool(m) >> r) & 1u;
        const Rectangle card{20 + float(col) * 122, 230, 116, 92};
        ui::panel(card, owned ? pal::Floor : pal::Panel);
        atlas().draw(relicSprite(RelicId(r)), card.x + 6, card.y + 6, owned ? pal::Gold : pal::Dim);
        ui::text(d.name, card.x + 26, card.y + 10, owned ? pal::Gold : pal::Bone);
        ui::textWrapped(d.desc, card.x + 6, card.y + 28, card.width - 12, pal::Dim);
        const std::string label = owned ? "Unlocked" : "Unlock " + std::to_string(d.unlockCost);
        if (ui::button({card.x + 6, card.y + 68, card.width - 12, 18}, label, !owned && m.embers >= d.unlockCost)) {
            if (buyRelicUnlock(m, RelicId(r))) {
                note_ = std::string(d.name) + " can now appear in runs.";
                app.saveProgress();
            }
        }
        ++col;
    }
    if (!note_.empty()) ui::text(note_, 160, 336, pal::Gold);
    if (ui::button({20, 332, 120, 20}, "Back")) app.toTitle();
}

// ---- Codex ------------------------------------------------------------------

void CodexScene::update(App& app, float) {
    if (IsKeyPressed(KEY_ESCAPE)) app.toTitle();
}

void CodexScene::draw(App& app) {
    const Meta& m = app.meta;
    ui::text("Codex", 20, 12, pal::Gold, 20);
    const char* tabs[] = {"Bestiary", "Relics", "Board twists"};
    for (int i = 0; i < 3; ++i)
        if (ui::button({140 + float(i) * 110, 12, 104, 20}, tabs[i], tab_ != i)) tab_ = i;

    if (tab_ == 0) {
        for (int k = 0; k < kMonsterKinds; ++k) {
            const MonsterKind kind = MonsterKind(k);
            const MonsterDef& d = monsterDef(kind);
            const bool seen = (m.seenMonsters >> k) & 1u;
            const Rectangle card{20 + float(k % 3) * 202, 40 + float(k / 3) * 70, 196, 64};
            ui::panel(card);
            atlas().draw(monsterSprite(kind), card.x + 6, card.y + 6, seen ? monsterColor(kind) : pal::Line, 2);
            if (!seen) {
                ui::text("???", card.x + 46, card.y + 10, pal::Dim);
                ui::text("Not yet met.", card.x + 46, card.y + 24, pal::Line);
                continue;
            }
            std::string title = d.name;
            if (d.boss) title += ((m.bossesBeaten >> k) & 1u) ? "  (slain)" : "  (boss)";
            ui::text(title, card.x + 46, card.y + 6, d.boss ? pal::Gold : pal::Bone);
            std::string stats = "Power " + std::to_string(d.power) + "  Cost " + std::to_string(d.hitCost);
            if (d.trait != Trait::None) stats += std::string("  ") + traitDesc(d.trait);
            ui::text(stats, card.x + 46, card.y + 18, pal::Dim);
            ui::text("Slain: " + std::to_string(m.kills[size_t(k)]), card.x + 46, card.y + 30, pal::Dim);
            if (ui::hover(card)) ui::setTooltip(d.name, d.blurb);
        }
    } else if (tab_ == 1) {
        for (int r = 0; r < kRelicKinds; ++r) {
            const RelicDef& d = relicDef(RelicId(r));
            const bool seen = (m.seenRelics >> r) & 1u;
            const Rectangle card{20 + float(r % 3) * 202, 44 + float(r / 3) * 70, 196, 64};
            ui::panel(card);
            atlas().draw(relicSprite(RelicId(r)), card.x + 6, card.y + 6, seen ? pal::Gold : pal::Line);
            ui::text(seen ? d.name : "???", card.x + 26, card.y + 10, seen ? pal::Gold : pal::Dim);
            ui::textWrapped(seen ? d.desc : "Not yet found.", card.x + 6, card.y + 28, card.width - 12,
                            seen ? pal::Dim : pal::Line);
        }
    } else {
        for (int i = 1; i < kModifierKinds; ++i) {
            const ModifierDef& d = modifierDef(Modifier(i));
            const bool seen = (m.seenModifiers >> i) & 1u;
            const Rectangle card{20, 44 + float(i - 1) * 50, 600, 44};
            ui::panel(card);
            ui::text(seen ? d.name : "???", card.x + 8, card.y + 8, seen ? pal::Blue : pal::Dim);
            ui::text(seen ? d.desc : "Not yet encountered.", card.x + 8, card.y + 24, seen ? pal::Dim : pal::Line);
        }
        ui::text("Runs " + std::to_string(m.runs) + "   Wins " + std::to_string(m.wins) + "   Deepest floor " +
                     std::to_string(m.bestLayer + 1) + "   Max heat " + std::to_string(m.maxHeat),
                 20, 260, pal::Bone);
    }
    if (ui::button({20, 332, 120, 20}, "Back")) app.toTitle();
}

} // namespace flagd
