// Shop, rest site, treasure room, and the end-of-run screen.
#include "core/content/balance.h"
#include "game/render/atlas.h"
#include "game/render/ui.h"
#include "game/scenes/scenes.h"

namespace flagd {

namespace {

constexpr float kRoomX = 172;

void roomHeader(const std::string& title, const std::string& sub) {
    ui::text(title, kRoomX, 12, pal::Gold, 20);
    ui::text(sub, kRoomX, 38, pal::Dim);
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
        atlas().draw(icon, card.x + 8, card.y + 8, o.sold ? pal::Line : o.relic ? pal::Gold : pal::Bone, 2);
        ui::text(name, card.x + 46, card.y + 10, o.sold ? pal::Dim : pal::Bone);
        ui::text(o.relic ? "Relic" : "Item", card.x + 46, card.y + 23, pal::Dim);
        ui::textWrapped(desc, card.x + 8, card.y + 44, card.width - 16, pal::Dim);
        const bool canAfford = run.player.gold >= o.price;
        const std::string label = o.sold ? "Sold" : "Buy  " + std::to_string(o.price) + "g";
        if (ui::button({card.x + 8, card.y + 90, card.width - 16, 18}, label, !o.sold && canAfford)) {
            if (buy(run, o)) note_ = std::string("Bought ") + name + ".";
            else note_ = "Your bag is full.";
        }
    }
    if (!note_.empty()) ui::text(note_, kRoomX + 136, 326, pal::Gold);
    if (leaveButton()) app.toMap();
    ui::playerPanel(run, nullptr, -1);
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
    ui::playerPanel(run, nullptr, -1);
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
        ui::text(r.name, 330, y + 4, pal::Gold);
        ui::textWrapped(r.desc, 330, y + 18, 200, pal::Bone);
        y += 50;
    }
    atlas().draw(spr::Coin, 290, y, pal::Gold, 2);
    ui::text("+" + std::to_string(loot_.gold) + " gold", 330, y + 10, pal::Gold);
    if (leaveButton("Continue")) app.toMap();
    ui::playerPanel(*app.game, nullptr, -1);
}

// ---- End of run -------------------------------------------------------------

void EndScene::update(App&, float dt) { time_ += dt; }

void EndScene::draw(App& app) {
    const Run& run = *app.game;
    const Player& p = run.player;
    const float cx = ui::kCanvasW / 2.0f;
    if (run.victory) {
        atlas().draw(spr::Trophy, cx - 24, 30, pal::Gold, 3);
        ui::textCentered("VICTORY", cx, 88, pal::Gold, 30);
        ui::textCentered("The Burrower is no more. The depths are yours.", cx, 124, pal::Bone);
    } else {
        atlas().draw(spr::Skull, cx - 24, 30, pal::Red, 3);
        ui::textCentered("DEFEAT", cx, 88, pal::Red, 30);
        ui::textCentered("Another flag planted in the dark.", cx, 124, pal::Bone);
    }

    const Rectangle box{cx - 140, 150, 280, 130};
    ui::panel(box);
    const std::pair<const char*, std::string> stats[] = {
        {"Floors completed", std::to_string(p.floorsCleared)},
        {"Monsters slain", std::to_string(p.kills)},
        {"Tiles revealed", std::to_string(p.tilesRevealed)},
        {"Gold earned", std::to_string(p.goldEarned)},
        {"Level reached", std::to_string(p.level)},
        {"Relics", std::to_string(p.relics.size())},
        {"Seed", std::to_string(run.seed)},
    };
    for (int i = 0; i < 7; ++i) {
        const float y = box.y + 10 + float(i) * 16;
        ui::text(stats[i].first, box.x + 14, y, pal::Dim);
        ui::text(stats[i].second, box.x + box.width - 14 - float(ui::textWidth(stats[i].second)), y, pal::Bone);
    }
    if (ui::button({cx - 70, 300, 140, 24}, "Back to title") || (time_ > 0.5f && IsKeyPressed(KEY_ENTER)))
        app.toTitle();
}

} // namespace flagd
