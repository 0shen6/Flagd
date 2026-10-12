#include <cstring>

#include "game/app.h"
#include "game/render/ui.h"
#include "game/scenes/scenes.h"

namespace {

using flagd::App;

void fakeClick(float x, float y, bool right = false) {
    auto& in = flagd::ui::input();
    in.mouse = {x, y};
    in.left = !right;
    in.right = right;
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

// `flagd --shots <dir>`: plays a scripted tour and saves a screenshot of each screen.
std::vector<flagd::ShotStep> shotScript() {
    using namespace flagd;
    std::vector<ShotStep> s;
    int f = 5;
    auto shot = [&](std::function<void(App&)> setup, std::function<void(App&)> interact, const char* name) {
        s.push_back({f, std::move(setup), ""});
        if (interact) s.push_back({f + 3, std::move(interact), ""});
        s.push_back({f + 6, [](App&) { hoverAt(402, 166); }, name});
        f += 8;
    };
    shot(nullptr, nullptr, "01_title.png");
    shot([](App& a) { a.go(std::make_unique<ClassSelectScene>(a)); }, nullptr, "02_class_select.png");
    shot([](App& a) { a.startRun(20261012, ClassId::Warrior, 0); }, nullptr, "03_map.png");
    shot([](App& a) { a.enterNode(a.game->choices()[0]); }, [](App&) { fakeClick(402, 166); }, "04_board.png");
    shot([](App& a) { openTwistBoard(a, Modifier::Hex, ClassId::Warrior); }, [](App&) { fakeClick(402, 166); },
         "05_hex.png");
    shot([](App& a) { openTwistBoard(a, Modifier::Wrap, ClassId::Warrior); }, [](App&) { fakeClick(402, 166); },
         "06_wrap.png");
    shot([](App& a) { openTwistBoard(a, Modifier::Fog, ClassId::Warrior); }, [](App&) { fakeClick(402, 166); },
         "07_fog.png");
    shot([](App& a) { openTwistBoard(a, Modifier::Liars, ClassId::Seer); }, [](App&) { fakeClick(402, 166); },
         "08_liars_seer.png");
    shot([](App& a) { openBoss(a, MonsterKind::FogWyrm); }, [](App&) { fakeClick(402, 182); }, "09_fog_wyrm.png");
    shot([](App& a) { openBoss(a, MonsterKind::HiveMother); }, [](App&) { fakeClick(402, 182); }, "10_hive.png");
    shot([](App& a) { a.go(std::make_unique<EventScene>(3)); }, nullptr, "11_event.png");
    shot(
        [](App& a) {
            a.meta.embers = 120;
            a.go(std::make_unique<CampScene>());
        },
        nullptr, "12_camp.png");
    shot(
        [](App& a) {
            a.meta.seenMonsters = 0x3FF;
            a.meta.seenRelics = 0x7F;
            a.meta.kills[0] = 12;
            a.go(std::make_unique<CodexScene>());
        },
        nullptr, "13_codex.png");
    shot(
        [](App& a) {
            a.game->over = a.game->victory = true;
            a.game->deepestLayer = 7;
            a.game->player.goldEarned = 200;
            a.toEnd();
        },
        nullptr, "14_end.png");
    return s;
}

} // namespace

int main(int argc, char** argv) {
    flagd::App app;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "--shots") == 0) app.setShotScript(argv[i + 1], shotScript());
    return app.loop();
}
