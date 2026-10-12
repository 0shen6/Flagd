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

void App::toTitle() {
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
    canvas_ = LoadRenderTexture(ui::kCanvasW, ui::kCanvasH);
    SetTextureFilter(canvas_.texture, TEXTURE_FILTER_POINT);
    scene_ = std::make_unique<TitleScene>();

    int frame = 0;
    while (!WindowShouldClose() && !quit_) {
        if (pending_) scene_ = std::move(pending_);
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
        if (IsKeyPressed(KEY_M)) audio::toggleMute();

        // Map the real mouse onto the integer-scaled canvas.
        const float sw = float(GetScreenWidth());
        const float sh = float(GetScreenHeight());
        float scale = std::min(sw / ui::kCanvasW, sh / ui::kCanvasH);
        if (scale >= 1.0f) scale = std::floor(scale);
        const float ox = (sw - ui::kCanvasW * scale) / 2;
        const float oy = (sh - ui::kCanvasH * scale) / 2;
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

        BeginTextureMode(canvas_);
        ClearBackground(pal::Bg);
        scene_->update(*this, dt);
        scene_->draw(*this);
        ui::drawTooltip();
        EndTextureMode();
        if (!shotName.empty()) {
            Image img = LoadImageFromTexture(canvas_.texture);
            ImageFlipVertical(&img);
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
