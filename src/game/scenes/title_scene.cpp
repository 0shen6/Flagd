// Title menu and class / heat / seed selection.
#include <cmath>
#include <ctime>

#include "core/rng.h"
#include "game/audio.h"
#include "game/render/atlas.h"
#include "game/render/ui.h"
#include "game/scenes/scenes.h"

namespace flagd {

namespace {

std::string randomSeedText() {
    uint64_t x = uint64_t(std::time(nullptr)) ^ uint64_t(GetTime() * 1e6);
    return std::to_string(splitmix64(x) % 1000000000ull);
}

// Draws text with the sprite sheet's 16x16 letter glyphs.
void bigText(const std::string& s, float cx, float y, float scale, Color c) {
    const float advance = 10 * scale;
    float x = cx - advance * float(s.size()) / 2 - 3 * scale;
    for (char ch : s) {
        if (ch >= 'A' && ch <= 'M') atlas().draw({35 + (ch - 'A'), 18}, x, y, c, scale);
        else if (ch >= 'N' && ch <= 'Z') atlas().draw({35 + (ch - 'N'), 19}, x, y, c, scale);
        x += advance;
    }
}

Spr classSprite(ClassId c) {
    switch (c) {
        case ClassId::Warrior: return spr::Helmet;
        case ClassId::Sapper: return {45, 9};
        case ClassId::Seer: return {37, 11};
        case ClassId::Gambler: return {46, 15};
        case ClassId::Count: break;
    }
    return spr::Helmet;
}

} // namespace

// ---- Title ------------------------------------------------------------------

void TitleScene::update(App& app, float dt) {
    time_ += dt;
    if (IsKeyPressed(KEY_ENTER)) app.go(std::make_unique<ClassSelectScene>(app));
}

void TitleScene::draw(App& app) {
    const float cx = ui::kCanvasW / 2.0f;
    bigText("FLAGD", cx, 34, 4, pal::Bone);
    ui::textCentered("a minesweeper roguelite", cx, 104, pal::Dim);

    const MonsterKind parade[] = {MonsterKind::Slime, MonsterKind::Bat,  MonsterKind::Skeleton, MonsterKind::Keg,
                                  MonsterKind::Wisp,  MonsterKind::Golem, MonsterKind::Spider,  MonsterKind::Burrower};
    for (int i = 0; i < 8; ++i) {
        const float bob = std::sin(time_ * 2.5f + float(i)) > 0 ? 0.0f : 2.0f;
        atlas().draw(monsterSprite(parade[i]), cx - 140 + float(i) * 36, 124 + bob, monsterColor(parade[i]), 2);
    }

    const float bx = cx - 80;
    if (ui::button({bx, 180, 160, 24}, "New Run")) app.go(std::make_unique<ClassSelectScene>(app));
    if (ui::button({bx, 210, 160, 24}, "Camp (upgrades)")) app.go(std::make_unique<CampScene>());
    if (ui::button({bx, 240, 160, 24}, "Codex")) app.go(std::make_unique<CodexScene>());
    if (ui::button({bx, 270, 160, 24}, "Quit")) app.quit();

    const Meta& m = app.meta;
    atlas().draw(spr::Ember, cx - 90, 304, pal::Orange);
    ui::text(std::to_string(m.embers) + " embers", cx - 72, 308, pal::Orange);
    ui::text("Runs " + std::to_string(m.runs) + "  Wins " + std::to_string(m.wins), cx + 10, 308, pal::Dim);
    ui::textCentered(std::string("F11 fullscreen   M ") + (audio::muted() ? "unmute" : "mute"), cx, 340, pal::Line);
}

// ---- Class select -----------------------------------------------------------

ClassSelectScene::ClassSelectScene(App&) : seedText_(randomSeedText()) {}

void ClassSelectScene::update(App& app, float dt) {
    time_ += dt;
    int ch = GetCharPressed();
    while (ch > 0) {
        if (ch >= '0' && ch <= '9' && seedText_.size() < 12) seedText_ += char(ch);
        ch = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !seedText_.empty()) seedText_.pop_back();
    if (IsKeyPressed(KEY_ESCAPE)) app.toTitle();
    if (IsKeyPressed(KEY_ENTER) && !seedText_.empty() && classUnlocked(app.meta, cls_))
        app.startRun(std::stoull(seedText_), cls_, heat_);
}

void ClassSelectScene::draw(App& app) {
    const Meta& m = app.meta;
    ui::text("Choose your delver", 20, 12, pal::Gold, 20);

    for (int i = 0; i < kClassKinds; ++i) {
        const ClassId c = ClassId(i);
        const ClassDef& d = classDef(c);
        const bool unlocked = classUnlocked(m, c);
        const Rectangle card{20 + float(i) * 152, 44, 144, 160};
        const bool sel = c == cls_;
        ui::panel(card, sel ? pal::PanelHi : pal::Panel, sel ? pal::Gold : ui::hover(card) ? pal::Bone : pal::Line);
        atlas().draw(classSprite(c), card.x + 8, card.y + 8, unlocked ? pal::Bone : pal::Line, 2);
        ui::text(d.name, card.x + 46, card.y + 12, unlocked ? pal::Gold : pal::Dim, 20);
        if (unlocked) {
            ui::text("HP " + std::to_string(d.maxHp) + "   XP " + std::to_string(d.xpPercent) + "%", card.x + 8,
                     card.y + 44, pal::Bone);
            ui::textWrapped(d.desc, card.x + 8, card.y + 60, card.width - 16, pal::Dim);
        } else {
            ui::text("LOCKED", card.x + 8, card.y + 44, pal::Red);
            ui::textWrapped(d.unlockHint, card.x + 8, card.y + 60, card.width - 16, pal::Dim);
        }
        if (ui::clicked(card) && unlocked) cls_ = c;
    }

    // Heat
    const Rectangle heatBox{20, 214, 400, 56};
    ui::panel(heatBox);
    ui::text("HEAT", heatBox.x + 8, heatBox.y + 8, pal::Orange);
    if (ui::button({heatBox.x + 50, heatBox.y + 5, 18, 16}, "<", heat_ > 0)) --heat_;
    ui::textCentered(std::to_string(heat_), heatBox.x + 82, heatBox.y + 8, heat_ > 0 ? pal::Orange : pal::Bone);
    if (ui::button({heatBox.x + 96, heatBox.y + 5, 18, 16}, ">", heat_ < m.maxHeat)) ++heat_;
    std::string effects = heat_ == 0 ? "No heat. Win a run to unlock Heat 1." : "";
    for (int h = 1; h <= heat_; ++h) effects += std::string(h > 1 ? " " : "") + heatDesc(h);
    ui::textWrapped(effects, heatBox.x + 124, heatBox.y + 8, heatBox.width - 132, pal::Dim);
    if (heat_ > 0)
        ui::text("+" + std::to_string(15 * heat_) + "% embers", heatBox.x + 8, heatBox.y + 36, pal::Orange);
    else if (m.maxHeat > 0)
        ui::text("Up to heat " + std::to_string(m.maxHeat), heatBox.x + 8, heatBox.y + 36, pal::Dim);

    // Seed + start
    ui::text("Seed", 20, 290, pal::Dim);
    const Rectangle field{54, 286, 120, 18};
    ui::panel(field, pal::Floor, pal::Bone);
    const bool caret = std::fmod(time_, 1.0f) < 0.5f;
    ui::text(seedText_ + (caret ? "_" : ""), field.x + 5, field.y + 4, pal::Bone);
    if (ui::button({180, 286, 70, 18}, "Random")) seedText_ = randomSeedText();

    if (ui::button({440, 230, 180, 30}, "Begin the descent", !seedText_.empty() && classUnlocked(m, cls_)))
        app.startRun(std::stoull(seedText_), cls_, heat_);
    if (ui::button({440, 286, 180, 20}, "Back")) app.toTitle();
}

} // namespace flagd
