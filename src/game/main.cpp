#include <cstring>

#include "game/app.h"
#include "game/render/ui.h"
#include "game/scenes/scenes.h"

namespace {

// `flagd --shots <dir>`: plays a short scripted run and saves screenshots.
std::vector<flagd::ShotStep> shotScript() {
    using flagd::App;
    auto click = [](float x, float y, bool right = false) {
        return [=](App&) {
            auto& in = flagd::ui::input();
            in.mouse = {x, y};
            in.left = !right;
            in.right = right;
        };
    };
    auto hoverAt = [](float x, float y) {
        return [=](App&) { flagd::ui::input().mouse = {x, y}; };
    };
    using namespace flagd;
    return {
        {5, nullptr, "01_title.png"},
        {6, [](App& a) { a.startRun(20261012); }, ""},
        {10, hoverAt(242, 52), "02_map.png"},
        {11, [](App& a) { a.enterNode(a.game->choices()[0]); }, ""},
        {15, hoverAt(402, 182), "03_board_start.png"},
        {16, click(402, 182), ""},
        {20, hoverAt(60, 150), "04_board_opened.png"},
        {21, click(530, 280, true), ""},
        {22, click(274, 280), ""},
        {23, click(530, 70), ""},
        {26, hoverAt(402, 182), "05_board_after_clicks.png"},
        {27, [](App& a) { a.game->player.gold = 120; a.go(std::make_unique<ShopScene>(a, 5)); }, ""},
        {31, hoverAt(600, 340), "06_shop.png"},
        {32, [](App& a) { a.go(std::make_unique<RestScene>()); }, ""},
        {36, nullptr, "07_rest.png"},
        {37, [](App& a) { a.go(std::make_unique<TreasureScene>(a, 6)); }, ""},
        {41, nullptr, "08_treasure.png"},
        {42, [](App& a) { a.go(std::make_unique<BoardScene>(a, a.game->map.bossId())); }, ""},
        {46, click(402, 182), ""},
        {50, hoverAt(402, 182), "09_boss.png"},
        {51, [](App& a) { a.game->over = a.game->victory = true; a.toEnd(); }, ""},
        {55, nullptr, "10_end.png"},
    };
}

} // namespace

int main(int argc, char** argv) {
    flagd::App app;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "--shots") == 0) app.setShotScript(argv[i + 1], shotScript());
    return app.loop();
}
