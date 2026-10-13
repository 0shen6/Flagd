#include "game/app.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "game/audio.h"
#include "game/render/atlas.h"
#include "game/render/ui.h"
#include "game/scenes/scenes.h"

namespace flagd {

namespace {

struct WindowPreset {
    int w, h;
};
constexpr WindowPreset kPresets[] = {{960, 540}, {1280, 720}, {1600, 900}, {1920, 1080}};
constexpr int kPresetCount = int(sizeof(kPresets) / sizeof(kPresets[0]));

} // namespace

void App::setFullscreen(bool on) {
    wantFullscreen_ = on ? 1 : 0;
    meta.fullscreen = on;
    saveProgress();
}

void App::setWindowPreset(int index) {
    meta.windowPreset = ((index % kPresetCount) + kPresetCount) % kPresetCount;
    wantPreset_ = meta.windowPreset;
    meta.fullscreen = false;
    saveProgress();
}

void App::setPixelPerfect(bool on) {
    meta.pixelPerfect = on;
    saveProgress();
}

// Our own borderless fullscreen. raylib's toggle left the drawing surface at
// the old size (the game drew into a corner and the rest of the screen stayed
// black). The window is made one pixel taller than the monitor so Windows
// doesn't switch it into exclusive "fullscreen optimizations" mode, which blinks.
void App::enterFullscreen() {
    savedPos_ = GetWindowPosition();
    savedW_ = GetScreenWidth();
    savedH_ = GetScreenHeight();
    const int mon = GetCurrentMonitor();
    const Vector2 mp = GetMonitorPosition(mon);
    SetWindowState(FLAG_WINDOW_UNDECORATED);
    SetWindowPosition(int(mp.x), int(mp.y));
    SetWindowSize(GetMonitorWidth(mon), GetMonitorHeight(mon) + 1);
    fullscreen_ = true;
}

void App::leaveFullscreen() {
    ClearWindowState(FLAG_WINDOW_UNDECORATED);
    SetWindowSize(savedW_ > 0 ? savedW_ : 1280, savedH_ > 0 ? savedH_ : 720);
    SetWindowPosition(int(savedPos_.x), int(savedPos_.y));
    fullscreen_ = false;
}

void App::applyWindowRequests() {
    if (wantPreset_ >= 0) {
        if (fullscreen_) leaveFullscreen();
        const WindowPreset p = kPresets[wantPreset_];
        const int mon = GetCurrentMonitor();
        const int w = std::min(p.w, GetMonitorWidth(mon));
        const int h = std::min(p.h, GetMonitorHeight(mon) - 60);  // leave room for the taskbar and title bar
        const Vector2 mp = GetMonitorPosition(mon);
        SetWindowSize(w, h);
        SetWindowPosition(int(mp.x) + (GetMonitorWidth(mon) - w) / 2, int(mp.y) + (GetMonitorHeight(mon) - h) / 2);
        wantPreset_ = -1;
    }
    if (wantFullscreen_ >= 0) {
        const bool on = wantFullscreen_ == 1;
        wantFullscreen_ = -1;
        if (on && !fullscreen_) enterFullscreen();
        if (!on && fullscreen_) leaveFullscreen();
    }
}

void App::setMuted(bool on) {
    if (audio::muted() != on) audio::toggleMute();
    meta.muted = on;
    saveProgress();
}

float App::drawOptions(float x, float y) {
    if (ui::button({x, y, 200, 20}, std::string("Fullscreen: ") + (fullscreen_ ? "On" : "Off")))
        setFullscreen(!fullscreen_);
    const WindowPreset& p = kPresets[std::clamp(meta.windowPreset, 0, kPresetCount - 1)];
    if (ui::button({x, y + 24, 200, 20}, "Window: " + std::to_string(p.w) + " x " + std::to_string(p.h)))
        setWindowPreset(meta.windowPreset + 1);
    if (ui::button({x, y + 48, 200, 20},
                   std::string("Scaling: ") + (meta.pixelPerfect ? "Pixel-perfect" : "Fill window")))
        setPixelPerfect(!meta.pixelPerfect);
    if (ui::button({x, y + 72, 200, 20}, std::string("Sound: ") + (audio::muted() ? "Off" : "On")))
        setMuted(!audio::muted());
    ui::text("Drag the window edges to resize.", x, y + 98, pal::Dim);
    ui::text("F11 fullscreen, M sound.", x, y + 110, pal::Dim);
    return 122;
}

void App::drawPause() {
    DrawRectangle(0, 0, ui::kCanvasW, ui::kCanvasH, Fade(BLACK, 0.6f));
    const Rectangle r{210, 40, 220, 280};
    ui::panel(r, pal::Bg, pal::Bone);
    ui::textCentered("Paused", r.x + r.width / 2, r.y + 10, pal::Gold, 20);
    const float x = r.x + 10;
    if (ui::button({x, r.y + 38, 200, 22}, "Resume")) paused_ = false;
    drawOptions(x, r.y + 68);
    if (ui::button({x, r.y + 196, 200, 22}, "Abandon run")) {
        paused_ = false;
        game->over = true;
        toEnd();
    }
    if (ui::button({x, r.y + 224, 200, 22}, "Quit game")) quit();
    ui::textCentered("Esc to resume", r.x + r.width / 2, r.y + 258, pal::Line);
}

void App::handleHudItems(int clickedSlot, int discardSlot) {
    if (!game) return;
    Player& p = game->player;
    if (discardSlot >= 0 && discardSlot < int(p.items.size())) {
        toast(std::string("Discarded the ") + itemDef(p.items[size_t(discardSlot)]).name + ".");
        discardItem(p, discardSlot);
        audio::play(Sfx::Flag);
    }
    if (clickedSlot >= 0 && clickedSlot < int(p.items.size())) {
        const ItemId id = p.items[size_t(clickedSlot)];
        const std::string what = useItemOutside(p, clickedSlot);
        if (!what.empty()) {
            toast(what);
            audio::play(Sfx::Heal);
        } else {
            toast(usableAnywhere(id) ? std::string("You're already at full health.")
                                     : std::string("The ") + itemDef(id).name + " can only be used on a board.");
            audio::play(Sfx::Error);
        }
    }
}

void App::toTitle() {
    paused_ = false;
    game.reset();
    go(std::make_unique<TitleScene>());
}

void App::toMap() { go(std::make_unique<MapScene>()); }

void App::toEnd() {
    if (game && !runBanked_) {
        lastResult = recordRun(meta, *game);
        runBanked_ = true;
        saveProgress();
    }
    go(std::make_unique<EndScene>());
}

void App::startRun(uint64_t seed, ClassId cls, int heat) {
    game = flagd::startRun(meta, seed, cls, heat);
    runBanked_ = false;
    lastResult = {};
    toMap();
}

std::string App::savePath() const {
    if (!shotDir_.empty()) return shotDir_ + "/save.txt";
    const char* appData = std::getenv("APPDATA");
    const std::filesystem::path dir =
        appData ? std::filesystem::path(appData) / "Flagd" : std::filesystem::path(GetApplicationDirectory());
    return (dir / "save.txt").string();
}

void App::loadProgress() {
    std::ifstream in(savePath());
    if (!in) return;
    std::stringstream text;
    text << in.rdbuf();
    if (!loadMeta(text.str(), meta)) TraceLog(LOG_WARNING, "Flagd: could not read save file, starting fresh");
}

void App::saveProgress() {
    if (!shotDir_.empty()) return;  // screenshot tours never touch saves
    const std::filesystem::path path = savePath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path);
    out << saveMeta(meta);
}

void App::enterNode(int nodeId) {
    if (!game || !travel(*game, nodeId)) return;
    switch (game->node(nodeId).type) {
        case NodeType::Normal:
        case NodeType::Elite:
        case NodeType::Boss: go(std::make_unique<BoardScene>(*this, nodeId)); break;
        case NodeType::Shop: go(std::make_unique<ShopScene>(*this, nodeId)); break;
        case NodeType::Rest: go(std::make_unique<RestScene>()); break;
        case NodeType::Treasure: go(std::make_unique<TreasureScene>(*this, nodeId)); break;
        case NodeType::Event: go(std::make_unique<EventScene>(nodeId)); break;
    }
}

int App::loop() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(ui::kCanvasW * 2, ui::kCanvasH * 2, "Flagd");
    SetWindowMinSize(ui::kCanvasW, ui::kCanvasH);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    const std::string sheet = std::string(GetApplicationDirectory()) + "assets/sheet.png";
    if (FileExists(sheet.c_str()) && atlas().load(sheet.c_str())) {
        Image icon = LoadImage(sheet.c_str());
        ImageCrop(&icon, {17 * 16, 7 * 16, 16, 16});
        ImageResizeNN(&icon, 64, 64);
        ImageColorTint(&icon, pal::Red);
        SetWindowIcon(icon);
        UnloadImage(icon);
    } else {
        TraceLog(LOG_WARNING, "Flagd: sprite sheet missing, using placeholders");
    }

    audio::init();
    loadProgress();
    if (meta.windowPreset != 1) wantPreset_ = std::clamp(meta.windowPreset, 0, kPresetCount - 1);
    if (meta.fullscreen) wantFullscreen_ = 1;
    if (meta.muted && !audio::muted()) audio::toggleMute();
    canvas_ = LoadRenderTexture(ui::kCanvasW, ui::kCanvasH);
    SetTextureFilter(canvas_.texture, TEXTURE_FILTER_POINT);
    scene_ = std::make_unique<TitleScene>();

    int frame = 0;
    while (!WindowShouldClose() && !quit_) {
        if (pending_) scene_ = std::move(pending_);
        if (IsKeyPressed(KEY_F11)) setFullscreen(!fullscreen_);
        if (IsKeyPressed(KEY_M)) setMuted(!audio::muted());
        applyWindowRequests();

        // Map the real mouse onto the scaled canvas (whole steps when pixel-perfect).
        const float sw = float(GetScreenWidth());
        const float sh = float(GetScreenHeight());
        float scale = std::min(sw / ui::kCanvasW, sh / ui::kCanvasH);
        if (scale >= 1.0f && meta.pixelPerfect) scale = std::floor(scale);
        const float ox = std::floor((sw - ui::kCanvasW * scale) / 2);
        const float oy = std::floor((sh - ui::kCanvasH * scale) / 2);
        const Vector2 m = GetMousePosition();
        auto& in = ui::input();
        in.mouse = {(m.x - ox) / scale, (m.y - oy) / scale};
        in.left = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        in.right = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        in.middle = IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE);

        std::string shotName;
        for (const ShotStep& step : shotSteps_)
            if (step.frame == frame) {
                if (step.action) step.action(*this);
                if (!step.shot.empty()) shotName = step.shot;
            }
        if (!shotSteps_.empty() && frame > shotSteps_.back().frame) quit_ = true;
        ++frame;

        const float dt = std::min(GetFrameTime(), 0.05f);
        shake_ = std::max(0.0f, shake_ - dt);

        if (IsKeyPressed(KEY_ESCAPE) && game && scene_->pausable() && !scene_->wantsEscape()) paused_ = !paused_;
        if (!scene_->pausable()) paused_ = false;

        BeginTextureMode(canvas_);
        ClearBackground(pal::Bg);
        if (paused_) {
            // Draw the scene frozen underneath, with input going to the pause menu only.
            const ui::Input real = ui::input();
            ui::input() = {{-1000, -1000}, false, false, false};
            scene_->draw(*this);
            ui::input() = real;
            drawPause();
        } else {
            scene_->update(*this, dt);
            scene_->draw(*this);
        }
        toastAge_ += dt;
        if (toastAge_ < 2.5f && !toast_.empty()) {
            const float w = float(ui::textWidth(toast_)) + 16;
            const float a = std::min(1.0f, (2.5f - toastAge_) * 2);
            DrawRectangleRec({(ui::kCanvasW - w) / 2, 316, w, 16}, Fade(pal::Bg, a));
            DrawRectangleLinesEx({(ui::kCanvasW - w) / 2, 316, w, 16}, 1, Fade(pal::Gold, a));
            ui::textCentered(toast_, ui::kCanvasW / 2.0f, 319, Fade(pal::Gold, a));
        }
        ui::drawTooltip();
        EndTextureMode();
        if (!shotName.empty()) {
            Image img = LoadImageFromTexture(canvas_.texture);
            ImageFlipVertical(&img);
            ImageResizeNN(&img, ui::kCanvasW * 2, ui::kCanvasH * 2);  // crisp 2x for docs
            ExportImage(img, (shotDir_ + "/" + shotName).c_str());
            UnloadImage(img);
        }

        BeginDrawing();
        ClearBackground(BLACK);
        Vector2 jitter{0, 0};
        if (shake_ > 0) {
            const float mag = scale * 2.0f * std::min(1.0f, shake_ * 4);
            jitter = {float(GetRandomValue(-1, 1)) * mag, float(GetRandomValue(-1, 1)) * mag};
        }
        DrawTexturePro(canvas_.texture, {0, 0, float(ui::kCanvasW), -float(ui::kCanvasH)},
                       {ox + jitter.x, oy + jitter.y, ui::kCanvasW * scale, ui::kCanvasH * scale}, {0, 0}, 0,
                       WHITE);
        EndDrawing();
    }

    scene_.reset();
    pending_.reset();
    UnloadRenderTexture(canvas_);
    atlas().unload();
    audio::shutdown();
    CloseWindow();
    return 0;
}

} // namespace flagd
