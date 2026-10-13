#include <cstring>

#include "game/app.h"
#include "game/render/ui.h"
#include "game/scenes/scenes.h"

namespace {

using flagd::App;

void fakeClick(float x, float y) {
    auto& in = flagd::ui::input();
    in.mouse = {x, y};
    in.left = true;
}

void hoverAt(float x, float y) { flagd::ui::input().mouse = {x, y}; }

// Opens a board for one of the run's first-layer nodes with a chosen twist.
void openTwistBoard(App& a, flagd::Modifier mod, flagd::ClassId cls) {
    const int node = a.game->map.layer(0)[0];
    a.game->map.nodes[size_t(node)].modifier = mod;
    a.game->player.cls = cls;
    a.go(std::make_unique<flagd::BoardScene>(a, node));
}

void openBoss(App& a, flagd::MonsterKind boss) {
    a.game->map.bossKind = boss;
    a.go(std::make_unique<flagd::BoardScene>(a, a.game->map.bossId()));
}

// `flagd --shots <dir>`: plays a scripted tour and saves a 2x screenshot of
// each screen (the README images come from here). Saves are never written.
std::vector<flagd::ShotStep> shotScript() {
    using namespace flagd;
    std::vector<ShotStep> s;
    int f = 5;
    // setup -> (optional) clicks, one per frame -> screenshot.
    auto shot = [&](std::function<void(App&)> setup, std::vector<Vector2> clicks, const char* name,
                    Vector2 hover = {402, 166}) {
        s.push_back({f, std::move(setup), ""});
        int t = f + 3;
        for (Vector2 c : clicks) s.push_back({t++, [c](App&) { fakeClick(c.x, c.y); }, ""});
        s.push_back({t + 3, [hover](App&) { hoverAt(hover.x, hover.y); }, name});
        f = t + 6;
    };
    const Vector2 gotIt{370, 207};   // dismisses a board's intro popup
    const Vector2 center{402, 166};  // a cell near the middle of small boards
    shot(nullptr, {}, "title.png");
    shot([](App& a) { a.go(std::make_unique<ClassSelectScene>(a)); }, {}, "class_select.png");
    shot([](App& a) { a.startRun(20261012, ClassId::Warrior, 0); }, {}, "map.png", {242, 52});
    shot([](App& a) { a.enterNode(a.game->choices()[0]); }, {center, {258 + 32 * 8 + 16, 54 + 16}}, "board.png");
    shot([](App& a) { openTwistBoard(a, Modifier::Hex, ClassId::Warrior); }, {gotIt, center}, "hex.png");
    shot([](App& a) { openTwistBoard(a, Modifier::Fog, ClassId::Warrior); }, {gotIt, center}, "fog.png");
    shot([](App& a) { openTwistBoard(a, Modifier::Liars, ClassId::Seer); }, {gotIt, center}, "liars_seer.png");
    shot([](App& a) { openBoss(a, MonsterKind::FogWyrm); }, {}, "boss_intro.png");
    shot([](App& a) { openBoss(a, MonsterKind::Burrower); }, {gotIt, {402, 182}}, "boss.png", {402, 182});
    shot(
        [](App& a) {
            a.game->actCleared = true;
            a.go(std::make_unique<BossRewardScene>());
        },
        {}, "boss_reward.png", {250, 120});
    shot([](App& a) { a.go(std::make_unique<EventScene>(3)); }, {}, "event.png");
    shot(
        [](App& a) {
            Player& p = a.game->player;
            p.relics.clear();
            for (int r = 0; r < kRelicKinds; ++r) p.relics.push_back(RelicId(r));
            p.maxItems = 5;
            p.items = {ItemId::Potion, ItemId::Bomb, ItemId::Compass, ItemId::Whetstone, ItemId::Elixir};
            openTwistBoard(a, Modifier::None, ClassId::Warrior);
        },
        {center}, "full_hud.png", {12, 260});
    shot([](App& a) { a.toMap(); }, {}, "full_hud_map.png", {90, 280});
    shot(
        [](App& a) {
            a.game->act = 3;
            openBoss(a, MonsterKind::HiveMother);
        },
        {gotIt, {402, 182}}, "boss_act3.png", {0, 0});
    shot([](App& a) { a.setPaused(true); }, {}, "pause.png", {0, 0});
    s.push_back({f, [](App& a) { a.setPaused(false); }, ""});
    f += 3;
    shot(
        [](App& a) {
            a.meta.embers = 120;
            a.go(std::make_unique<CampScene>());
        },
        {}, "camp.png", {0, 0});
    shot(
        [](App& a) {
            a.meta.seenMonsters = 0x3FF;
            a.meta.seenRelics = 0x7FFFF;
            a.meta.seenModifiers = 0x1E;
            a.meta.kills[0] = 12;
            a.go(std::make_unique<CodexScene>());
        },
        {{300, 22}}, "codex.png", {0, 0});
    shot([](App& a) { a.go(std::make_unique<OptionsScene>()); }, {}, "options.png", {0, 0});
    shot(
        [](App& a) {
            a.game->over = a.game->victory = true;
            a.game->deepestLayer = 23;
            a.game->player.goldEarned = 200;
            a.toEnd();
        },
        {}, "end.png", {0, 0});
    return s;
}

// `flagd --fullscreen-test <dir>`: toggles fullscreen on and off a few times
// on a busy screen and logs the window state each second (for flicker checks).
std::vector<flagd::ShotStep> fullscreenScript() {
    using namespace flagd;
    std::vector<ShotStep> s;
    s.push_back({5, [](App& a) {
                     a.startRun(42, ClassId::Warrior, 0);
                 }, ""});
    s.push_back({10, [](App& a) { a.enterNode(a.game->choices()[0]); }, ""});
    s.push_back({14, [](App&) { fakeClick(402, 166); }, ""});
    int f = 30;
    for (int cycle = 0; cycle < 3; ++cycle) {
        s.push_back({f, [](App& a) { a.setFullscreen(true); }, ""});
        for (int t = 0; t < 240; t += 30)  // wiggle the mouse over tiles (hover outlines, tooltips)
            s.push_back({f + t + 15, [t](App&) { hoverAt(260 + float(t), 120 + float(t % 90)); }, ""});
        for (int t = 60; t <= 240; t += 60)
            s.push_back({f + t, [](App&) {
                             TraceLog(LOG_INFO, "FSTEST screen %dx%d render %dx%d undecorated=%d resizable=%d",
                                      GetScreenWidth(), GetScreenHeight(), GetRenderWidth(), GetRenderHeight(),
                                      int(IsWindowState(FLAG_WINDOW_UNDECORATED)),
                                      int(IsWindowState(FLAG_WINDOW_RESIZABLE)));
                         }, ""});
        s.push_back({f + 250, [](App& a) { a.setFullscreen(false); }, ""});
        s.push_back({f + 300, [](App&) {
                         TraceLog(LOG_INFO, "FSTEST windowed %dx%d resizable=%d", GetScreenWidth(), GetScreenHeight(),
                                  int(IsWindowState(FLAG_WINDOW_RESIZABLE)));
                     }, ""});
        f += 330;
    }
    // Finally an odd window size, as if dragged by the edges.
    s.push_back({f + 10, [](App&) { SetWindowSize(1000, 640); }, ""});
    s.push_back({f + 40, [](App&) {
                     TraceLog(LOG_INFO, "FSTEST resized %dx%d render %dx%d", GetScreenWidth(), GetScreenHeight(),
                              GetRenderWidth(), GetRenderHeight());
                 }, ""});
    s.push_back({f + 100, nullptr, ""});
    return s;
}

} // namespace

int main(int argc, char** argv) {
    flagd::App app;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], "--shots") == 0) app.setShotScript(argv[i + 1], shotScript());
        if (std::strcmp(argv[i], "--fullscreen-test") == 0) app.setShotScript(argv[i + 1], fullscreenScript());
    }
    return app.loop();
}
