#include <algorithm>
#include <cmath>

#include "game/render/atlas.h"
#include "game/render/ui.h"
#include "game/scenes/scenes.h"

namespace flagd {

namespace {

const char* nodeBlurb(NodeType t) {
    switch (t) {
        case NodeType::Normal: return "A board of monsters. Earn XP, gold, and maybe an item.";
        case NodeType::Elite: return "Denser board with a Spider extra. Its chest holds a relic.";
        case NodeType::Shop: return "Spend gold on items and relics.";
        case NodeType::Rest: return "Heal, or train to raise max HP.";
        case NodeType::Treasure: return "A free relic and some gold.";
        case NodeType::Event: return "Something unusual. A choice awaits.";
        case NodeType::Boss: return "The guardian of the depths. Slay it to win the run.";
    }
    return "";
}

bool contains(const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }

} // namespace

Vector2 MapScene::nodePos(const Run& run, int id) const {
    const MapNode& n = run.node(id);
    const float y = 52 + float(n.layer) * 39;  // the descent runs downward
    if (n.type == NodeType::Boss) return {392, y + 4};
    return {242 + float(n.col) * 100, y};
}

void MapScene::update(App& app, float dt) {
    time_ += dt;
    if (clickedNode_ >= 0) {
        const int id = clickedNode_;
        clickedNode_ = -1;
        app.enterNode(id);
    }
}

void MapScene::draw(App& app) {
    const Run& run = *app.game;
    const auto choices = run.choices();
    ui::text("Choose your path", 172, 8, pal::Bone);
    ui::text(std::string("Each step leads deeper. The ") + monsterDef(run.map.bossKind).name + " waits at the bottom.",
             172, 21, pal::Dim);
    ui::playerPanel(run, nullptr, -1);

    // Edges
    for (int id = 0; id < int(run.map.nodes.size()); ++id) {
        const Vector2 a = nodePos(run, id);
        for (int nx : run.node(id).next) {
            const Vector2 b = nodePos(run, nx);
            Color c = pal::Line;
            if (contains(run.path, id) && contains(run.path, nx)) c = pal::Gold;
            else if (id == run.current && contains(choices, nx)) c = pal::Bone;
            DrawLineEx(a, b, 1, c);
        }
    }

    // Nodes
    for (int id = 0; id < int(run.map.nodes.size()); ++id) {
        const MapNode& n = run.node(id);
        const Vector2 c = nodePos(run, id);
        const bool boss = n.type == NodeType::Boss;
        const float half = boss ? 16 : 11;
        const Rectangle r{c.x - half, c.y - half, half * 2, half * 2};
        const bool available = contains(choices, id);
        const bool visited = contains(run.path, id);
        const bool over = available && ui::hover(r);
        Color border = pal::Line;
        if (available) border = std::fmod(time_, 1.0f) < 0.5f ? pal::Gold : pal::Bone;
        if (id == run.current) border = pal::Gold;
        ui::panel(r, over ? pal::PanelHi : visited ? pal::Floor : pal::Panel, border);
        Color tint = nodeColor(n.type);
        if (!available && !visited && id != run.current) tint = Fade(tint, 0.45f);
        if (visited && id != run.current) tint = pal::Line;
        const Spr icon = boss ? monsterSprite(run.map.bossKind) : nodeSprite(n.type);
        atlas().draw(icon, r.x + half - (boss ? 16 : 8), r.y + half - (boss ? 16 : 8), tint, boss ? 2.0f : 1.0f);
        if (n.modifier != Modifier::None) {
            // Badge: first letter of the twist (W, H, F, L).
            const std::string badge(1, modifierDef(n.modifier).name[0]);
            DrawRectangle(int(r.x + r.width - 6), int(r.y - 4), 9, 10, pal::Blue);
            ui::text(badge, r.x + r.width - 5, r.y - 4, pal::Bg);
        }
        if (ui::hover(r)) {
            std::string body = nodeBlurb(n.type);
            if (n.modifier != Modifier::None)
                body += std::string(" Twist: ") + modifierDef(n.modifier).name + ". " + modifierDef(n.modifier).desc;
            if (boss) body = std::string("The ") + monsterDef(run.map.bossKind).name + ". " +
                             monsterDef(run.map.bossKind).blurb + " Slay it to win the run.";
            ui::setTooltip(nodeTypeName(n.type), body);
        }
        if (available && ui::clicked(r)) clickedNode_ = id;
    }

    // Legend, inside the side panel
    ui::text("LEGEND", 10, 214, pal::Bone);
    const NodeType legend[] = {NodeType::Normal, NodeType::Elite, NodeType::Shop, NodeType::Rest, NodeType::Treasure,
                               NodeType::Event};
    for (int i = 0; i < 6; ++i) {
        const float y = 228 + float(i) * 17;
        atlas().draw(nodeSprite(legend[i]), 10, y, nodeColor(legend[i]));
        ui::text(nodeTypeName(legend[i]), 30, y + 4, pal::Dim);
    }
    ui::text("Blue badge: board twist", 10, 332, pal::Blue);
}

} // namespace flagd
