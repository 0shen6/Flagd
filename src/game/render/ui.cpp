#include "game/render/ui.h"

#include <algorithm>
#include <sstream>
#include <vector>

#include "core/content/balance.h"
#include "game/render/atlas.h"

namespace flagd::ui {

namespace {

std::string g_tipTitle;
std::string g_tipBody;

std::vector<std::string> wrap(const std::string& s, float maxWidth) {
    std::vector<std::string> lines;
    std::istringstream words(s);
    std::string word;
    std::string line;
    while (words >> word) {
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && float(textWidth(candidate)) > maxWidth) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

} // namespace

Input& input() {
    static Input in;
    return in;
}

bool hover(Rectangle r) { return CheckCollisionPointRec(input().mouse, r); }
bool clicked(Rectangle r) { return input().left && hover(r); }

void text(const std::string& s, float x, float y, Color c, int size) {
    DrawText(s.c_str(), int(x), int(y), size, c);
}

void textCentered(const std::string& s, float cx, float y, Color c, int size) {
    text(s, cx - float(textWidth(s, size)) / 2.0f, y, c, size);
}

int textWidth(const std::string& s, int size) { return MeasureText(s.c_str(), size); }

int textWrapped(const std::string& s, float x, float y, float maxWidth, Color c) {
    const auto lines = wrap(s, maxWidth);
    for (size_t i = 0; i < lines.size(); ++i) text(lines[i], x, y + float(i) * 11.0f, c);
    return int(lines.size()) * 11;
}

void panel(Rectangle r, Color fill, Color border) {
    DrawRectangleRec(r, fill);
    DrawRectangleLinesEx(r, 1, border);
}

void bar(Rectangle r, float frac, Color fill, Color back) {
    DrawRectangleRec(r, back);
    frac = std::clamp(frac, 0.0f, 1.0f);
    DrawRectangleRec({r.x, r.y, r.width * frac, r.height}, fill);
    DrawRectangleLinesEx(r, 1, pal::Line);
}

bool button(Rectangle r, const std::string& label, bool enabled) {
    const bool over = enabled && hover(r);
    const bool down = over && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    panel(r, !enabled ? pal::Floor : over ? pal::PanelHi : pal::Panel, over ? pal::Bone : pal::Line);
    const float dy = down ? 1.0f : 0.0f;
    textCentered(label, r.x + r.width / 2, r.y + (r.height - 10) / 2 + dy, enabled ? pal::Bone : pal::Dim);
    return enabled && clicked(r);
}

void setTooltip(const std::string& title, const std::string& body) {
    g_tipTitle = title;
    g_tipBody = body;
}

void drawTooltip() {
    if (g_tipTitle.empty() && g_tipBody.empty()) return;
    const float w = 170;
    const auto lines = wrap(g_tipBody, w - 10);
    const float h = 8 + (g_tipTitle.empty() ? 0 : 12) + float(lines.size()) * 11 + 2;
    float x = input().mouse.x + 10;
    float y = input().mouse.y + 10;
    if (x + w > kCanvasW - 2) x = input().mouse.x - w - 6;
    if (y + h > kCanvasH - 2) y = kCanvasH - 2 - h;
    panel({x, y, w, h}, pal::Bg, pal::Bone);
    float ty = y + 5;
    if (!g_tipTitle.empty()) {
        text(g_tipTitle, x + 5, ty, pal::Gold);
        ty += 12;
    }
    for (const auto& l : lines) {
        text(l, x + 5, ty, pal::Bone);
        ty += 11;
    }
    g_tipTitle.clear();
    g_tipBody.clear();
}

std::string floorLabel(const Run& run, int nodeId) {
    if (nodeId < 0) return "The Descent";
    const MapNode& n = run.node(nodeId);
    return "Floor " + std::to_string(n.layer + 1) + "/" + std::to_string(run.map.layers) + "  " +
           nodeTypeName(n.type);
}

HudResult playerPanel(const Run& run, const Encounter* enc, int selectedSlot) {
    HudResult res;
    const Player& p = run.player;
    const float x = 4;
    const float w = 146;
    panel({x, 4, w, 352});
    const float ix = x + 6;
    const float iw = w - 12;

    atlas().draw(spr::Helmet, ix, 8, pal::Bone);
    text(classDef(p.cls).name, ix + 20, 12, pal::Bone);
    const std::string lv = "Lv " + std::to_string(p.level);
    text(lv, ix + iw - float(textWidth(lv)), 12, pal::Gold);

    // HP
    bar({ix, 30, iw, 12}, float(p.hp) / float(std::max(1, p.maxHp)), pal::Red, pal::Maroon);
    textCentered("HP " + std::to_string(p.hp) + "/" + std::to_string(p.maxHp), ix + iw / 2, 31, pal::Bone);
    if (hover({ix, 30, iw, 12}))
        setTooltip("Health", p.halfLevelHeal ? "Fights cost HP. Leveling up raises max HP and heals half of it."
                                             : "Fights cost HP. Leveling up raises max HP and heals you fully.");
    // XP
    const int need = balance::xpToNext(p.level);
    bar({ix, 46, iw, 6}, float(p.xp) / float(need), pal::Blue);
    if (hover({ix, 44, iw, 10}))
        setTooltip("Experience", std::to_string(p.xp) + " / " + std::to_string(need) + " XP to level " +
                                     std::to_string(p.level + 1) + ". Slay monsters to earn XP.");

    // Gold, key, shield
    atlas().draw(spr::Coin, ix - 2, 56, pal::Gold);
    text(std::to_string(p.gold), ix + 16, 60, pal::Gold);
    float sx = ix + 60;
    if (p.shield > 0) {
        atlas().draw(spr::ShieldIcon, sx, 56, pal::Blue);
        text("x" + std::to_string(p.shield), sx + 16, 60, pal::Blue);
        if (hover({sx, 56, 32, 16})) setTooltip("Shield", "The next fight costs no HP.");
        sx += 40;
    }
    if (enc && enc->hasKey) {
        atlas().draw(spr::Key, sx, 56, pal::Gold);
        if (hover({sx, 56, 16, 16})) setTooltip("Key", "Opens this board's chest.");
    }

    float y = 76;
    // Status effects
    std::string status;
    if (p.poison > 0) status += "Poisoned (" + std::to_string(p.poison) + ")  ";
    if (enc && enc->hexed) status += "Cursed: half XP";
    if (!status.empty()) {
        text(status, ix, y, pal::Purple);
        if (hover({ix, y, iw, 10}))
            setTooltip("Status", "Poison costs 1 HP per turn (a Potion or rest cures it). A curse halves XP until you "
                                 "leave this board.");
        y += 12;
    }
    text(floorLabel(run, run.current), ix, y, pal::Dim);
    y += 12;
    if (enc) {
        std::string info = "Turn " + std::to_string(enc->turn);
        if (p.has(RelicId::TenthStep)) info += "  Safe step in " + std::to_string(tenthStepCountdown(*enc));
        text(info, ix, y, pal::Dim);
        y += 14;

        text("MONSTERS", ix, y, pal::Bone);
        y += 12;
        // Two columns: sprite, power, alive/total. Hover for details.
        const auto alive = enc->board.livingCounts(false);
        const auto placed = enc->board.totalCounts();
        int col = 0;
        for (int k = 0; k < kMonsterKinds; ++k) {
            const int total = std::max(enc->params.counts[size_t(k)], placed[size_t(k)]);
            if (total == 0) continue;
            const MonsterKind kind = MonsterKind(k);
            const MonsterDef& def = monsterDef(kind);
            const float cx = ix + float(col) * 70;
            const Rectangle cell{cx, y, 64, 16};
            atlas().draw(monsterSprite(kind), cx, y, monsterColor(kind));
            text(std::to_string(def.power), cx + 18, y + 4, threatColor(def.power));
            const std::string cnt =
                std::to_string(enc->generated ? alive[size_t(k)] : total) + "/" + std::to_string(total);
            text(cnt, cx + 62 - float(textWidth(cnt)), y + 4, pal::Dim);
            if (hover(cell)) {
                std::string body = std::string(def.blurb) + " Power " + std::to_string(def.power) +
                                   ". Attack: -" + std::to_string(def.hitCost) + " HP";
                if (def.boss) {
                    body += " per hit, " + std::to_string(def.hits) + " hits.";
                } else if (def.ambushDamage > 0) {
                    body += " (-" + std::to_string(def.ambushDamage) + " if it goes off unmarked).";
                } else {
                    body += " (-" + std::to_string(def.hitCost * balance::kAmbushMultiplier) + " if unmarked).";
                }
                body += " +" + std::to_string(def.xp) + " XP per hit.";
                setTooltip(def.name, body);
            }
            if (++col == 2) {
                col = 0;
                y += 17;
            }
        }
        if (col != 0) y += 17;
        y += 4;
    } else {
        y += 4;
    }

    text("RELICS", ix, y, pal::Bone);
    y += 12;
    if (p.relics.empty()) text("none yet", ix, y + 3, pal::Dim);
    for (size_t i = 0; i < p.relics.size(); ++i) {
        const float rx = ix + float(i % 7) * 19;
        const float ry = y + float(i / 7) * 18;
        atlas().draw(relicSprite(p.relics[i]), rx, ry, pal::Gold);
        if (hover({rx, ry, 16, 16})) setTooltip(relicDef(p.relics[i]).name, relicDef(p.relics[i]).desc);
    }
    y += 18 * float((p.relics.size() + 6) / 7 + (p.relics.empty() ? 1 : 0)) + 6;

    text("ITEMS", ix, y, pal::Bone);
    y += 12;
    static const char* kKeys[] = {"Q", "W", "E", "R"};
    for (int i = 0; i < balance::kInventorySlots; ++i) {
        const Rectangle slot{ix + float(i) * 34, y, 30, 28};
        const bool sel = i == selectedSlot;
        panel(slot, sel ? pal::PanelHi : pal::Floor, sel ? pal::Gold : pal::Line);
        text(kKeys[i], slot.x + 2, slot.y + 1, pal::Dim);
        if (i < int(p.items.size())) {
            const ItemId id = p.items[size_t(i)];
            atlas().draw(itemSprite(id), slot.x + 9, slot.y + 8, pal::Bone);
            if (hover(slot)) {
                std::string body = itemDef(id).desc;
                if (enc) body += itemDef(id).targeted ? " Click it, then a tile." : " Click to use.";
                setTooltip(itemDef(id).name, body);
            }
            if (clicked(slot)) res.clickedSlot = i;
        }
    }
    y += 34;
    res.bottom = y;

    std::string footer = "Seed " + std::to_string(run.seed);
    if (run.heat > 0) footer += "  Heat " + std::to_string(run.heat);
    text(footer, ix, 342, run.heat > 0 ? pal::Orange : pal::Line);
    return res;
}

} // namespace flagd::ui
