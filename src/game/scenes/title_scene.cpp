#include <cmath>
#include <ctime>

#include "core/rng.h"
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

} // namespace

TitleScene::TitleScene() : seedText_(randomSeedText()) {}

void TitleScene::update(App& app, float dt) {
    time_ += dt;
    int ch = GetCharPressed();
    while (ch > 0) {
        if (ch >= '0' && ch <= '9' && seedText_.size() < 12) seedText_ += char(ch);
        ch = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !seedText_.empty()) seedText_.pop_back();
    if (IsKeyPressed(KEY_ENTER) && !seedText_.empty()) app.startRun(std::stoull(seedText_));
}

void TitleScene::draw(App& app) {
    const float cx = ui::kCanvasW / 2.0f;
    bigText("FLAGD", cx, 34, 4, pal::Bone);
    ui::textCentered("a minesweeper roguelite", cx, 104, pal::Dim);

    // A parade of the depths' residents.
    const MonsterKind parade[] = {MonsterKind::Slime, MonsterKind::Bat, MonsterKind::Skeleton, MonsterKind::Spider,
                                  MonsterKind::Burrower};
    for (int i = 0; i < 5; ++i) {
        const float bob = std::sin(time_ * 2.5f + float(i)) > 0 ? 0.0f : 2.0f;
        atlas().draw(monsterSprite(parade[i]), cx - 90 + float(i) * 36, 122 + bob, monsterColor(parade[i]), 2);
    }

    // Class card
    const Rectangle card{70, 170, 230, 120};
    ui::panel(card);
    atlas().draw(spr::Helmet, card.x + 8, card.y + 8, pal::Bone, 2);
    const ClassDef& cls = classDef(ClassId::Warrior);
    ui::text(cls.name, card.x + 48, card.y + 12, pal::Gold, 20);
    ui::textWrapped(cls.desc, card.x + 8, card.y + 46, card.width - 16, pal::Bone);
    ui::text("More classes coming soon.", card.x + 8, card.y + 100, pal::Line);

    // How to play
    const Rectangle how{320, 170, 250, 120};
    ui::panel(how);
    ui::text("HOW TO PLAY", how.x + 8, how.y + 8, pal::Gold);
    const char* lines[] = {
        "Numbers show the total POWER of",
        "monsters around a tile.",
        "Mark monsters (RMB / 1 2 3 5), then",
        "click them to fight for XP.",
        "Unmarked surprises hit twice as hard.",
        "Level up to heal. Find the stairs.",
        "No board ever forces a guess.",
    };
    for (int i = 0; i < 7; ++i) ui::text(lines[i], how.x + 8, how.y + 22 + float(i) * 13, pal::Bone);

    // Seed + start
    ui::text("Seed", 70, 304, pal::Dim);
    const Rectangle field{104, 300, 120, 18};
    ui::panel(field, pal::Floor, pal::Bone);
    const bool caret = std::fmod(time_, 1.0f) < 0.5f;
    ui::text(seedText_ + (caret ? "_" : ""), field.x + 5, field.y + 4, pal::Bone);
    if (ui::button({230, 300, 70, 18}, "Random")) seedText_ = randomSeedText();
    if (ui::button({320, 298, 160, 22}, "Begin the descent", !seedText_.empty()))
        app.startRun(std::stoull(seedText_));
    if (ui::button({490, 298, 80, 22}, "Quit")) app.quit();
    ui::textCentered("F11 fullscreen", cx, 340, pal::Line);
}

} // namespace flagd
