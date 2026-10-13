// Shop, rest site, treasure room, and the end-of-run screen.
#include "core/content/balance.h"
#include "game/render/atlas.h"
#include "game/render/ui.h"
#include "game/scenes/scenes.h"

namespace flagd {

namespace {

constexpr float kRoomX = 172;

void roomHeader(const std::string& title, const std::string& sub) {
    ui::textFit(title, kRoomX, 12, 460, pal::Gold, 20);
    ui::textFit(sub, kRoomX, 38, 460, pal::Dim);
}

bool leaveButton(const char* label = "Leave") {
    return ui::button({kRoomX, 320, 120, 22}, label) || IsKeyPressed(KEY_ENTER);
}

} // namespace

// ---- Shop -------------------------------------------------------------------

ShopScene::ShopScene(App& app, int nodeId) : offers_(makeShop(*app.game, nodeId)) {}

void ShopScene::update(App&, float) {}

void ShopScene::draw(App& app) {
    Run& run = *app.game;
    roomHeader("Shop", "A hooded merchant spreads their wares on a crate.");
    for (size_t i = 0; i < offers_.size(); ++i) {
        ShopOffer& o = offers_[i];
        const Rectangle card{kRoomX + float(i % 3) * 152, 60 + float(i / 3) * 124, 144, 116};
        ui::panel(card, o.sold ? pal::Floor : pal::Panel);
        const Spr icon = o.relic ? relicSprite(RelicId(o.id)) : itemSprite(ItemId(o.id));
        const char* name = o.relic ? relicDef(RelicId(o.id)).name : itemDef(ItemId(o.id)).name;
        const char* desc = o.relic ? relicDef(RelicId(o.id)).desc : itemDef(ItemId(o.id)).desc;
        atlas().draw(icon, card.x + 8, card.y + 8,
                     o.sold ? pal::Line : o.relic ? pal::Gold : (!o.relic && ItemId(o.id) == ItemId::Bomb) ? pal::Red : pal::Bone, 2);
        ui::textFit(name, card.x + 46, card.y + 10, card.width - 50, o.sold ? pal::Dim : pal::Bone);
        ui::text(o.relic ? "Relic" : "Item", card.x + 46, card.y + 23, pal::Dim);
        ui::textWrapped(desc, card.x + 8, card.y + 44, card.width - 16, pal::Dim, 4);
        if (ui::hover(card)) ui::setTooltip(name, desc);
        const bool canAfford = run.player.gold >= o.price;
        const std::string label = o.sold ? "Sold" : "Buy  " + std::to_string(o.price) + "g";
        if (ui::button({card.x + 8, card.y + 90, card.width - 16, 18}, label, !o.sold && canAfford)) {
            if (buy(run, o)) note_ = std::string("Bought ") + name + ".";
            else note_ = "Your bag is full.";
        }
    }
    if (!note_.empty()) ui::textFit(note_, kRoomX + 136, 326, 320, pal::Gold);
    if (leaveButton()) app.toMap();
    {
        const auto hud = ui::playerPanel(run, nullptr, -1);
        app.handleHudItems(hud.clickedSlot, hud.discardSlot);
    }
}

// ---- Rest site --------------------------------------------------------------

void RestScene::update(App&, float) {}

void RestScene::draw(App& app) {
    Run& run = *app.game;
    Player& p = run.player;
    roomHeader("Rest Site", "A campfire crackles. Take a moment.");
    atlas().draw(nodeSprite(NodeType::Rest), 360, 70, pal::Orange, 4);

    const int heal = std::max(1, p.maxHp * balance::kRestHealPercent / 100);
    if (!done_) {
        const std::string healLabel = "Rest: heal " + std::to_string(std::min(heal, p.maxHp - p.hp)) + " HP";
        if (ui::button({260, 160, 180, 26}, healLabel, p.hp < p.maxHp)) {
            restHeal(p);
            done_ = true;
            note_ = "You feel restored.";
        }
        if (ui::button({260, 196, 180, 26}, "Train: +" + std::to_string(balance::kRestFortifyMaxHp) + " max HP")) {
            restFortify(p);
            done_ = true;
            note_ = "You feel tougher.";
        }
    } else {
        ui::textCentered(note_, 350, 170, pal::Green);
    }
    if (leaveButton(done_ ? "Continue" : "Skip")) app.toMap();
    {
        const auto hud = ui::playerPanel(run, nullptr, -1);
        app.handleHudItems(hud.clickedSlot, hud.discardSlot);
    }
}

// ---- Treasure ---------------------------------------------------------------

TreasureScene::TreasureScene(App& app, int nodeId) : loot_(takeTreasure(*app.game, nodeId)) {}

void TreasureScene::update(App&, float) {}

void TreasureScene::draw(App& app) {
    roomHeader("Treasure", "An unguarded chest. Suspicious, but generous.");
    atlas().draw(spr::Chest, 360, 70, pal::Brown, 4);
    float y = 150;
    if (loot_.hasRelic) {
        const RelicDef& r = relicDef(loot_.relic);
        atlas().draw(relicSprite(loot_.relic), 290, y, pal::Gold, 2);
        ui::textFit(r.name, 330, y + 4, 290, pal::Gold);
        ui::textWrapped(r.desc, 330, y + 18, 290, pal::Bone, 2);
        y += 50;
    }
    atlas().draw(spr::Coin, 290, y, pal::Gold, 2);
    ui::text("+" + std::to_string(loot_.gold) + " gold", 330, y + 10, pal::Gold);
    if (leaveButton("Continue")) app.toMap();
    {
        const auto hud = ui::playerPanel(*app.game, nullptr, -1);
        app.handleHudItems(hud.clickedSlot, hud.discardSlot);
    }
}

// ---- Boss reward ------------------------------------------------------------

void BossRewardScene::update(App&, float) {}

void BossRewardScene::draw(App& app) {
    Run& run = *app.game;
    roomHeader("Act " + std::to_string(run.act) + " cleared!",
               "Choose a relic from the guardian's hoard. Then the descent continues.");
    const auto choices = bossRelicChoices(run);
    bool chosen = false;
    for (size_t i = 0; i < choices.size() && !chosen; ++i) {
        const RelicDef& d = relicDef(choices[i]);
        const Rectangle card{kRoomX + float(i) * 152, 70, 144, 150};
        ui::panel(card, ui::hover(card) ? pal::PanelHi : pal::Panel, ui::hover(card) ? pal::Gold : pal::Line);
        atlas().draw(relicSprite(choices[i]), card.x + 56, card.y + 10, pal::Gold, 2);
        ui::textCentered(ui::fitted(d.name, card.width - 8), card.x + card.width / 2, card.y + 52, pal::Gold);
        ui::textWrapped(d.desc, card.x + 8, card.y + 70, card.width - 16, pal::Bone, 4);
        if (ui::button({card.x + 8, card.y + 124, card.width - 16, 20}, "Take")) {
            run.player.relics.push_back(choices[i]);
            chosen = true;
        }
    }
    if (choices.empty()) ui::text("The hoard is empty: you own every relic!", kRoomX, 90, pal::Dim);
    const int healPct = run.heat >= 3 ? balance::kActHealPercent / 2 : balance::kActHealPercent;
    ui::text("Entering act " + std::to_string(run.act + 1) + " heals " + std::to_string(healPct) + "% of missing HP.",
             kRoomX, 236, pal::Green);
    if (!chosen && ui::button({kRoomX, 320, 160, 22}, choices.empty() ? "Continue" : "Skip the relic")) chosen = true;
    if (chosen) {
        advanceAct(run);
        app.toMap();
    }
    {
        const auto hud = ui::playerPanel(run, nullptr, -1);
        app.handleHudItems(hud.clickedSlot, hud.discardSlot);
    }
}

// ---- Event ------------------------------------------------------------------

void EventScene::update(App&, float) {}

void EventScene::draw(App& app) {
    Run& run = *app.game;
    const EventId id = eventAt(run, nodeId_);
    const EventDef& d = eventDef(id);
    roomHeader(d.title, "");
    atlas().draw(spr::Question, 360, 56, pal::Blue, 4);
    ui::textWrapped(d.text, 220, 130, 300, pal::Bone, 4);
    if (outcome_.empty()) {
        for (int o = 0; o < 2; ++o) {
            const bool allowed = eventOptionAllowed(run, id, o);
            if (ui::button({220, 180 + float(o) * 30, 300, 24}, d.options[o], allowed))
                outcome_ = resolveEvent(run, nodeId_, o);
        }
    } else {
        ui::textWrapped(outcome_, 220, 186, 300, pal::Gold, 4);
        if (leaveButton("Continue")) app.toMap();
    }
    {
        const auto hud = ui::playerPanel(run, nullptr, -1);
        app.handleHudItems(hud.clickedSlot, hud.discardSlot);
    }
}

// ---- End of run -------------------------------------------------------------

void EndScene::update(App&, float dt) { time_ += dt; }

void EndScene::draw(App& app) {
    const Run& run = *app.game;
    const Player& p = run.player;
    const float cx = ui::kCanvasW / 2.0f;
    if (run.victory) {
        atlas().draw(spr::Trophy, cx - 16, 14, pal::Gold, 2);
        ui::textCentered("VICTORY", cx, 50, pal::Gold, 30);
        ui::textCentered(std::string("The ") + monsterDef(run.map.bossKind).name + " is no more. The depths are yours.",
                         cx, 84, pal::Bone);
    } else {
        atlas().draw(spr::Skull, cx - 16, 14, pal::Red, 2);
        ui::textCentered("DEFEAT", cx, 50, pal::Red, 30);
        ui::textCentered("Another flag planted in the dark.", cx, 84, pal::Bone);
    }

    const Rectangle box{cx - 140, 102, 280, 128};
    ui::panel(box);
    const std::pair<const char*, std::string> stats[8] = {
        {"Floors completed", std::to_string(p.floorsCleared)},
        {"Monsters slain", std::to_string(p.kills)},
        {"Tiles revealed", std::to_string(p.tilesRevealed)},
        {"Gold earned", std::to_string(p.goldEarned)},
        {"Level reached", std::to_string(p.level)},
        {"Relics", std::to_string(p.relics.size())},
        {"Seed", std::to_string(run.seed) + (run.heat ? "  heat " + std::to_string(run.heat) : "")},
        {"Class", classDef(p.cls).name},
    };
    for (int i = 0; i < 8; ++i) {
        const float y = box.y + 6 + float(i) * 15;
        ui::text(stats[i].first, box.x + 14, y, pal::Dim);
        ui::text(stats[i].second, box.x + box.width - 14 - float(ui::textWidth(stats[i].second)), y, pal::Bone);
    }
    // Meta rewards
    const RunResult& res = app.lastResult;
    float y = 242;
    atlas().draw(spr::Ember, cx - 60, y - 4, pal::Orange);
    ui::text("+" + std::to_string(res.embers) + " embers (" + std::to_string(app.meta.embers) + " total)", cx - 42, y,
             pal::Orange);
    for (const std::string& u : res.unlocks) {
        y += 12;
        ui::textCentered(u, cx, y, pal::Gold);
    }
    if (ui::button({cx - 70, 330, 140, 22}, "Back to title") ||
        (time_ > 0.5f && IsKeyPressed(KEY_ENTER)))
        app.toTitle();
}

} // namespace flagd
