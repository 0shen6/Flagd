#include <algorithm>
#include <cmath>

#include "core/content/balance.h"
#include "game/audio.h"
#include "game/render/atlas.h"
#include "game/render/ui.h"
#include "game/scenes/scenes.h"

namespace flagd {

namespace {

// Board area on the canvas (right of the HUD, room for flare labels).
constexpr float kAreaX = 172;
constexpr float kAreaY = 36;
constexpr float kAreaW = 460;
constexpr float kAreaH = 292;

bool isTargeted(const Player& p, int slot) {
    return slot >= 0 && slot < int(p.items.size()) && itemDef(p.items[size_t(slot)]).targeted;
}

void drawNumber(int n, Rectangle r, float scale, Color c) {
    if (n <= 0) return;
    if (n < 10) {
        atlas().draw(spr::Digit(n), r.x, r.y, c, scale);
        return;
    }
    const std::string s = std::to_string(n);
    const int size = int(10 * scale);
    ui::text(s, r.x + (r.width - float(ui::textWidth(s, size))) / 2, r.y + (r.height - float(size)) / 2 + scale,
             c, size);
}

void drawSmallCentered(const std::string& s, Rectangle r, Color c) {
    ui::text(s, r.x + (r.width - float(ui::textWidth(s))) / 2, r.y + (r.height - 10) / 2 + 1, c);
}

std::string bossName(MonsterKind k) { return std::string("The ") + monsterDef(k).name; }

} // namespace

BoardScene::BoardScene(App& app, int nodeId) : nodeId_(nodeId), enc_(beginEncounter(*app.game, nodeId)) {
    const bool hex = enc_.board.topology() == Topology::Hex;
    const float tilesW = float(enc_.params.width * 16) + (hex ? 8.0f : 0.0f);
    const float tilesH = float(enc_.params.height * 16);
    scale_ = std::max(1.0f, std::floor(std::min(kAreaW / tilesW, kAreaH / tilesH)));
    origin_ = {std::floor(kAreaX + (kAreaW - tilesW * scale_) / 2), std::floor(kAreaY + (kAreaH - tilesH * scale_) / 2)};
    const Modifier mod = enc_.params.modifier;
    if (enc_.params.boss) {
        const MonsterKind boss = app.game->map.bossKind;
        say(bossName(boss) + " waits below.", pal::Gold);
        popupTitle_ = bossName(boss);
        popupBody_ = std::string(monsterDef(boss).blurb) +
                     " Armor makes it immune at first. Uncovering the board breaks it: one point at 40%, 70% "
                     "and 95% of the safe tiles. Then strike.";
    } else if (mod != Modifier::None) {
        say(std::string("Twist: ") + modifierDef(mod).name, pal::Blue);
        const uint32_t bit = 1u << int(mod);
        if (!(app.meta.seenModifiers & bit) && !(app.game->player.seenModifiers & bit)) {
            popupTitle_ = std::string("New twist: ") + modifierDef(mod).name;
            popupBody_ = modifierDef(mod).desc;
        }
    } else {
        say("Click any tile to begin. The first click is always safe.", pal::Bone);
    }
}

Rectangle BoardScene::cellRect(Pos p) const {
    const float t = 16 * scale_;
    const float shift = (enc_.board.topology() == Topology::Hex && (p.y & 1)) ? t / 2 : 0;
    return {origin_.x + float(p.x) * t + shift, origin_.y + float(p.y) * t, t, t};
}

Vector2 BoardScene::cellCenter(Pos p) const {
    const Rectangle r = cellRect(p);
    return {r.x + r.width / 2, r.y + r.height / 2};
}

bool BoardScene::cellAt(Vector2 m, Pos& out) const {
    const float t = 16 * scale_;
    const int y = int(std::floor((m.y - origin_.y) / t));
    const float shift = (enc_.board.topology() == Topology::Hex && (y & 1)) ? t / 2 : 0;
    const int x = int(std::floor((m.x - origin_.x - shift) / t));
    if (x < 0 || y < 0 || x >= enc_.params.width || y >= enc_.params.height) return false;
    out = {x, y};
    return true;
}

void BoardScene::say(const std::string& msg, Color c) {
    // Several events from one action share the line while it fits.
    if (logAge_ < 0.05f && !log_.empty() && ui::textWidth(log_ + "  " + msg) <= int(kAreaW)) {
        log_ += "  " + msg;
    } else {
        log_ = msg;
    }
    logColor_ = c;
    logAge_ = 0;
}

void BoardScene::floatAt(Pos p, const std::string& s, Color c) {
    floats_.push_back({cellCenter(p), s, c, 0});
}

void BoardScene::act(App& app, const Action& a) {
    const bool wasGenerated = enc_.generated;
    if (a.type != ActionType::SetMark && a.type != ActionType::CycleMark) {
        // Every action starts a fresh line, so old news never sticks around.
        log_.clear();
        logAge_ = 1;
    }
    const auto events = applyAction(app.game->player, enc_, a);
    if (!wasGenerated && enc_.generated) {
        logAge_ = 1;
        say(enc_.params.boss ? "Uncover the board to break the boss's armor, then strike."
                             : "Find the stairs to descend. Clear the board for bonus gold.",
            pal::Bone);
        if (enc_.hexed) say("You carry a curse: XP is halved on this board.", pal::Purple);
        logAge_ = 0;
        audio::play(Sfx::Reveal);
    }
    react(app, events);
}

void BoardScene::react(App& app, const std::vector<Event>& events) {
    Pos last{};
    Sfx sound = Sfx::None;
    auto want = [&](Sfx s) { sound = std::max(sound, s); };  // the loudest event wins
    for (const Event& e : events) {
        if (e.type != EventType::XpGained && e.type != EventType::LevelUp && e.type != EventType::Heal &&
            e.type != EventType::BoardCleared && e.type != EventType::Descend && e.type != EventType::PoisonTick)
            last = e.pos;
        switch (e.type) {
            case EventType::Damage:
                floatAt(e.pos, "-" + std::to_string(e.amount), pal::Red);
                if (e.aux == 2) say("Your mark was a guess: ambush! -" + std::to_string(e.amount) + " HP.", pal::Red);
                else if (e.aux == 1) say("Ambush! -" + std::to_string(e.amount) + " HP.", pal::Red);
                else say("-" + std::to_string(e.amount) + " HP.", pal::Red);
                app.shake(e.aux ? 0.3f : 0.18f);
                flash_ = e.aux ? 0.4f : 0.25f;
                want(e.aux ? Sfx::Ambush : Sfx::Hit);
                break;
            case EventType::Heal:
                say("Healed " + std::to_string(e.amount) + " HP.", pal::Green);
                want(Sfx::Heal);
                break;
            case EventType::Gold:
                floatAt(e.pos, "+" + std::to_string(e.amount), pal::Gold);
                want(Sfx::Coin);
                break;
            case EventType::Kill:
                say(std::string("Slain: ") + monsterDef(MonsterKind(e.aux)).name + ".", pal::Bone);
                want(Sfx::Kill);
                break;
            case EventType::XpGained:
                floats_.push_back({{cellCenter(last).x, cellCenter(last).y - 8}, "+" + std::to_string(e.amount) + "xp",
                                   pal::Blue, 0});
                break;
            case EventType::LevelUp:
                say("LEVEL UP! Level " + std::to_string(e.amount) + ".", pal::Green);
                floats_.push_back({{cellCenter(last).x, cellCenter(last).y - 16}, "LEVEL UP", pal::Green, 0});
                want(Sfx::LevelUp);
                break;
            case EventType::KeyFound:
                floatAt(e.pos, "KEY", pal::Gold);
                say("Found the key!", pal::Gold);
                want(Sfx::Coin);
                break;
            case EventType::ChestOpened:
                say("The chest creaks open.", pal::Brown);
                want(Sfx::Coin);
                break;
            case EventType::NeedKey: say("Locked. The key is somewhere on this board.", pal::Dim); break;
            case EventType::ItemGained:
                say(std::string("Found: ") + itemDef(ItemId(e.aux)).name + ".", pal::Bone);
                break;
            case EventType::InventoryFull:
                say(std::string("Bag full: sold the ") + itemDef(ItemId(e.aux)).name + ".", pal::Dim);
                break;
            case EventType::RelicGained:
                say(std::string("Relic: ") + relicDef(RelicId(e.aux)).name + "!", pal::Gold);
                want(Sfx::LevelUp);
                break;
            case EventType::ItemUsed:
                say(std::string("Used ") + itemDef(ItemId(e.aux)).name + ".", pal::Bone);
                want(Sfx::Reveal);
                break;
            case EventType::ExitFound:
                say("Stairs found! Click them to descend, or keep exploring.", pal::Gold);
                break;
            case EventType::BoardCleared:
                say("Board cleared! +" + std::to_string(e.amount) + " gold.", pal::Gold);
                want(Sfx::LevelUp);
                break;
            case EventType::ShieldBlock: say("Your shield absorbs the blow.", pal::Blue); break;
            case EventType::Warded: say("Tenth Step: the monster shows itself instead of striking.", pal::Green); break;
            case EventType::WrongFlag: say("Wrong flag!", pal::Red); break;
            case EventType::Burrow:
                pulses_.push_back({e.pos, 0});
                say("The Burrower buries a tile. Dig it back up.", pal::Orange);
                want(Sfx::Burrow);
                break;
            case EventType::HintsPlaced:
                say(std::to_string(e.amount) + " hint tile(s) opened so you never have to guess.", pal::Blue);
                break;
            case EventType::Explosion:
                say("KABOOM! The keg blows the area open.", pal::Orange);
                app.shake(0.45f);
                want(Sfx::Explosion);
                break;
            case EventType::Poisoned:
                say("Poisoned for " + std::to_string(e.amount) + " turns!", pal::Purple);
                break;
            case EventType::PoisonTick: floats_.push_back({{80, 36}, "-1 poison", pal::Purple, 0}); break;
            case EventType::Cursed: say("Cursed! XP is halved on this board.", pal::Purple); break;
            case EventType::Asleep: say("It never woke up.", pal::Green); break;
            case EventType::Lucky:
                floatAt(e.pos, "LUCKY", pal::Green);
                say("A gamble paid off!", pal::Green);
                break;
            case EventType::FogLifts: say("The fog thins around the Wyrm.", pal::Bone); break;
            case EventType::Spawned:
                pulses_.push_back({e.pos, 0});
                say("The Hive Mother births a Wisp.", pal::Gold);
                break;
            case EventType::BossHit:
                say(bossName(app.game->map.bossKind) + " reels! " + std::to_string(e.amount) + " hit(s) left.",
                    pal::Gold);
                want(Sfx::Hit);
                break;
            case EventType::BossDefeated:
                say(bossName(app.game->map.bossKind) + " is slain!", pal::Gold);
                want(Sfx::Victory);
                break;
            case EventType::PlayerDied:
                say("You have fallen...", pal::Red);
                want(Sfx::Death);
                break;
            case EventType::Invalid:
                pulses_.push_back({e.pos, 0});
                want(Sfx::Error);
                break;
            case EventType::Descend: want(Sfx::Descend); break;
            case EventType::BossArmored:
                pulses_.push_back({e.pos, 0});
                say("Its armor holds (" + std::to_string(e.amount) + "). Uncover more: " + std::to_string(e.aux) +
                        "% of " + std::to_string(nextArmorBreak(enc_)) + "%.",
                    pal::Blue);
                want(Sfx::Error);
                break;
            case EventType::ArmorBroken:
                say(e.amount > 0 ? "Armor cracks! " + std::to_string(e.amount) + " left."
                                 : std::string("The armor shatters! Strike now."),
                    pal::Blue);
                app.shake(0.2f);
                want(Sfx::Explosion);
                break;
            case EventType::SecondWind: say("Second Wind! You cling on at 1 HP.", pal::Green); break;
            case EventType::Sensed: break;
        }
    }
    if (sound == Sfx::None && !events.empty()) sound = Sfx::Reveal;
    audio::play(sound);
    if (enc_.descended) {
        finishBoard(*app.game, enc_);
        app.toMap();
    }
}

void BoardScene::update(App& app, float dt) {
    time_ += dt;
    logAge_ += dt;
    flash_ = std::max(0.0f, flash_ - dt);
    for (auto& f : floats_) f.age += dt;
    std::erase_if(floats_, [](const FloatText& f) { return f.age > 1.1f; });
    for (auto& p : pulses_) p.second += dt;
    std::erase_if(pulses_, [](const auto& p) { return p.second > 0.6f; });

    if (enc_.lost || enc_.won || confirmExit_ || !popupTitle_.empty()) return;
    Player& pl = app.game->player;

    // Item hotkeys
    const int keys[] = {KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T};
    for (int i = 0; i < std::min(pl.maxItems, 5); ++i) {
        if (!IsKeyPressed(keys[i]) || i >= int(pl.items.size())) continue;
        if (isTargeted(pl, i)) selectedSlot_ = selectedSlot_ == i ? -1 : i;
        else act(app, {ActionType::UseItem, {}, Mark::None, i});
    }
    if (IsKeyPressed(KEY_ESCAPE)) selectedSlot_ = -1;
    if (selectedSlot_ >= int(pl.items.size())) selectedSlot_ = -1;

    Pos p;
    if (!cellAt(ui::input().mouse, p)) return;
    const Cell& c = enc_.board.at(p);
    const auto& in = ui::input();

    // Keyboard marks on the hovered tile.
    struct KeyMark { int key; int alt; Mark mark; };
    static constexpr KeyMark kMarks[] = {
        {KEY_ONE, KEY_KP_1, Mark::P1}, {KEY_TWO, KEY_KP_2, Mark::P2}, {KEY_THREE, KEY_KP_3, Mark::P3},
        {KEY_FIVE, KEY_KP_5, Mark::P5}, {KEY_F, KEY_F, Mark::Flag},   {KEY_ZERO, KEY_BACKSPACE, Mark::None},
    };
    for (const auto& km : kMarks)
        if (IsKeyPressed(km.key) || IsKeyPressed(km.alt)) {
            act(app, {ActionType::SetMark, p, km.mark});
            audio::play(Sfx::Flag);
        }

    if (in.right) {
        selectedSlot_ = -1;
        act(app, {ActionType::CycleMark, p});
        audio::play(Sfx::Flag);
        return;
    }
    if (in.left || in.middle || IsKeyPressed(KEY_SPACE)) {
        if (in.left && selectedSlot_ >= 0) {
            const int slot = selectedSlot_;
            selectedSlot_ = -1;
            act(app, {ActionType::UseItem, p, Mark::None, slot});
            return;
        }
        if (c.revealed && c.feature == Feature::Exit) {
            confirmExit_ = true;
            return;
        }
        act(app, {ActionType::Reveal, p});
    }
}

void BoardScene::drawCell(Pos p, bool hovered, bool seer) {
    const Board& b = enc_.board;
    const Cell& c = b.at(p);
    const Rectangle r = cellRect(p);
    const float s = scale_;
    const Monster* m = enc_.generated ? b.livingMonsterAt(p) : nullptr;

    if (c.revealed) {
        DrawRectangleRec(r, pal::Floor);
        DrawRectangleLinesEx(r, 1, Color{40, 37, 50, 255});
        const NumberView nv = b.view(p);
        if (m && m->visible && m->cells.size() == 1) {
            // A Wisp drifting over open ground; its tile's number stays readable.
            atlas().draw(monsterSprite(m->kind), r.x, r.y, monsterColor(m->kind), s);
            if (nv.shown > 0) ui::text(std::to_string(nv.shown), r.x + 1, r.y + 1, threatColor(nv.shown));
            return;
        }
        if (c.feature == Feature::Exit) {
            atlas().draw(spr::Exit, r.x, r.y, pal::Gold, s);
        } else if (c.feature == Feature::Chest) {
            atlas().draw(spr::Chest, r.x, r.y, c.chestOpen ? pal::Line : pal::Brown, s);
        } else if (nv.liar) {
            drawNumber(nv.shown, r, s, pal::Purple);
            ui::text("?", r.x + r.width - 6, r.y, pal::Purple);
        } else if (!nv.exact()) {
            DrawRectangleRec({r.x + 1, r.y + 1, r.width - 2, r.height - 2}, Color{44, 48, 60, 255});
            const std::string range = s >= 2 ? std::to_string(nv.lo) + "-" + (nv.hi >= 99 ? "" : std::to_string(nv.hi))
                                             : std::to_string(nv.lo) + "+";
            drawSmallCentered(range, r, Color{150, 170, 190, 255});
        } else {
            drawNumber(nv.shown, r, s, threatColor(nv.shown));
        }
        if (seer && nv.shown > 0) {
            // Second Sight: one pip per kind of hidden monster nearby.
            const uint32_t kinds = lurkingKinds(b, p);
            int i = 0;
            for (int k = 0; k < kMonsterKinds; ++k)
                if ((kinds >> k) & 1u) {
                    DrawRectangle(int(r.x + 1 + float(i) * 3 * s), int(r.y + r.height - 3 * s), int(2 * s), int(2 * s),
                                  monsterColor(MonsterKind(k)));
                    ++i;
                }
        }
        if (c.hint) DrawRectangle(int(r.x + 1), int(r.y + 1), int(2 * s), int(2 * s), pal::Blue);
        return;
    }

    if (m && m->visible) {
        DrawRectangleRec(r, Color{58, 30, 40, 255});
        DrawRectangleLinesEx(r, 1, pal::Maroon);
        if (m->cells.size() == 1) atlas().draw(monsterSprite(m->kind), r.x, r.y, monsterColor(m->kind), s);
        return;
    }

    // Hidden tile with a little bevel.
    DrawRectangleRec(r, hovered ? pal::HiddenHi : pal::Hidden);
    DrawRectangle(int(r.x), int(r.y), int(r.width), int(s), Color{96, 90, 116, 255});
    DrawRectangle(int(r.x), int(r.y), int(s), int(r.height), Color{96, 90, 116, 255});
    DrawRectangle(int(r.x), int(r.y + r.height - s), int(r.width), int(s), Color{34, 31, 42, 255});
    DrawRectangle(int(r.x + r.width - s), int(r.y), int(s), int(r.height), Color{34, 31, 42, 255});

    if (c.buried) {
        atlas().draw(spr::Rubble, r.x, r.y, pal::Brown, s);
        return;
    }
    if (c.sensed && c.mark == Mark::None) {
        const Spr icon = c.feature == Feature::Exit ? spr::Exit : c.feature == Feature::Chest ? spr::Chest : spr::Key;
        atlas().draw(icon, r.x, r.y, Fade(pal::Gold, 0.35f), s);
    }
    if (c.mark == Mark::Flag) {
        atlas().draw(spr::Flag, r.x, r.y, pal::Red, s);
    } else if (c.mark != Mark::None) {
        const int pw = markPower(c.mark);
        atlas().draw(spr::Flag, r.x, r.y, Fade(threatColor(pw), 0.45f), s);
        atlas().draw(spr::Digit(pw), r.x, r.y, threatColor(pw), s);
    }
}

void BoardScene::drawHoverInfo(const Player& pl) {
    Pos p;
    std::string info;
    const Board& b = enc_.board;
    if (isTargeted(pl, selectedSlot_)) {
        ui::textFit(std::string("Pick a tile for the ") + itemDef(pl.items[size_t(selectedSlot_)]).name +
                        ". Esc cancels.",
                    kAreaX, 334, kAreaW, pal::Gold);
        return;
    }
    if (!cellAt(ui::input().mouse, p)) {
        info = "LMB reveal/attack/chord   RMB mark   1 2 3 5 power mark   F flag   0 clear";
    } else if (!enc_.generated) {
        info = "Your first click is always safe and opens an area.";
    } else {
        const Cell& c = b.at(p);
        const Monster* m = b.livingMonsterAt(p);
        if (m && m->visible) {
            const MonsterDef& d = monsterDef(m->kind);
            info = std::string(d.name) + " (power " + std::to_string(d.power) + "). Click to attack: -" +
                   std::to_string(d.hitCost) + " HP, +" + std::to_string(d.xp) + " XP.";
            if (d.boss) {
                info += " Hits left: " + std::to_string(m->hp) + ".";
                if (enc_.bossArmor > 0) info = std::string(d.name) + " is armored. Uncover " +
                                               std::to_string(nextArmorBreak(enc_)) + "% of the board to crack it.";
            }
            if (d.trait == Trait::Dormant && b.surrounded(p)) info = std::string(d.name) + " sleeps soundly: free kill.";
        } else if (c.revealed) {
            const NumberView nv = b.view(p);
            if (c.feature == Feature::Exit) info = "Stairs down. Click to descend.";
            else if (c.feature == Feature::Chest)
                info = c.chestOpen ? "An empty chest." : enc_.hasKey ? "Chest. Click to open." : "Locked chest. Find the key.";
            if (nv.liar) {
                info = "A lying tile: its number is off by 1 or 2. Ignore it.";
            } else if (nv.hi > 0) {
                int known = 0;
                int hiddenLeft = 0;
                int seen[8];
                int seenCount = 0;
                b.forNeighbors(p, [&](Pos n) {
                    const Cell& nc = b.at(n);
                    if (b.isHidden(n)) {
                        if (nc.mark != Mark::None && nc.mark != Mark::Flag) known += markPower(nc.mark);
                        else if (nc.mark == Mark::None) ++hiddenLeft;
                        return;
                    }
                    const Monster* nm = b.livingMonsterAt(n);
                    if (!nm || !nm->visible) return;
                    for (int i = 0; i < seenCount; ++i)
                        if (seen[i] == nc.monster) return;
                    seen[seenCount++] = nc.monster;
                    known += nm->power;
                });
                if (!info.empty()) info += "  ";
                const std::string threat = nv.exact() ? std::to_string(nv.lo)
                                                      : std::to_string(nv.lo) + "-" +
                                                            (nv.hi >= 99 ? std::string("?") : std::to_string(nv.hi));
                info += "Threat " + threat + ": " + std::to_string(known) + " accounted, " +
                        std::to_string(hiddenLeft) + " unmarked tile(s) left.";
                if (known == nv.hi && hiddenLeft > 0) info += " Click to chord.";
            }
        } else if (c.buried) {
            info = "Buried by the Burrower. Known safe: click to dig it up.";
        } else if (c.mark != Mark::None) {
            info = "Marked. If you can prove a monster is here, clicking attacks it at normal cost; "
                   "if the mark is a guess, it's an ambush.";
        } else {
            info = "Unknown tile.";
        }
    }
    ui::textWrapped(info, kAreaX, 334, kAreaW, pal::Dim, 2);
}

void BoardScene::draw(App& app) {
    Run& run = *app.game;
    const Player& pl = run.player;
    const Board& b = enc_.board;

    // Header
    std::string header = ui::floorLabel(run, nodeId_);
    if (enc_.params.modifier != Modifier::None) header += std::string("  [") + modifierDef(enc_.params.modifier).name + "]";
    if (enc_.params.boss && enc_.generated && !enc_.won) {
        const MonsterKind boss = run.map.bossKind;
        if (enc_.bossArmor > 0)
            header += "    Armor " + std::to_string(enc_.bossArmor) + ": " + std::to_string(uncoveredPercent(b)) + "% / " +
                      std::to_string(nextArmorBreak(enc_)) + "%";
        if (boss == MonsterKind::Burrower) {
            const int every = balance::kBossBurrowEvery;
            header += "    Burrows in " + std::to_string(every - enc_.turn % every) + " turn(s)";
        } else if (boss == MonsterKind::HiveMother) {
            const int every = balance::kHiveSpawnEvery;
            header += "    Births a Wisp in " + std::to_string(every - enc_.turn % every) + " turn(s)";
        }
    }
    ui::textFit(header, kAreaX, 8, enc_.exitFound ? kAreaW - 90 : kAreaW, pal::Bone);
    if (ui::hover({kAreaX, 6, kAreaW, 12}) && enc_.params.modifier != Modifier::None)
        ui::setTooltip(modifierDef(enc_.params.modifier).name, modifierDef(enc_.params.modifier).desc);
    if (!log_.empty()) {
        // News fades after a few seconds; hover the line to read it in full.
        const float alpha = std::clamp(1.0f - (logAge_ - 3.5f) / 1.0f, 0.0f, 1.0f);
        if (alpha > 0) {
            const bool cut = ui::textFit(log_, kAreaX, 21, kAreaW, Fade(logColor_, alpha));
            if (cut && ui::hover({kAreaX, 19, kAreaW, 12})) ui::setTooltip("", log_);
        }
    }

    Pos hover{-1, -1};
    const bool hasHover = cellAt(ui::input().mouse, hover);
    const bool seer = pl.cls == ClassId::Seer;
    for (int y = 0; y < b.height(); ++y)
        for (int x = 0; x < b.width(); ++x) drawCell({x, y}, hasHover && hover == Pos{x, y}, seer);

    // Wrap-around boards: mark every edge so the connection is obvious.
    if (b.topology() == Topology::Wrap) {
        const Rectangle tl = cellRect({0, 0});
        const Rectangle br = cellRect({b.width() - 1, b.height() - 1});
        const Color c = Fade(pal::Blue, 0.6f + 0.3f * std::sin(time_ * 3));
        for (float x = tl.x; x < br.x + br.width; x += 6) {
            DrawRectangle(int(x), int(tl.y - 3), 3, 2, c);
            DrawRectangle(int(x), int(br.y + br.height + 1), 3, 2, c);
        }
        for (float y = tl.y; y < br.y + br.height; y += 6) {
            DrawRectangle(int(tl.x - 3), int(y), 2, 3, c);
            DrawRectangle(int(br.x + br.width + 1), int(y), 2, 3, c);
        }
    }

    // Multi-cell monsters (bosses) are drawn once at double size.
    for (const Monster& m : b.monsters()) {
        if (!m.alive || !m.visible || m.cells.size() < 2) continue;
        Pos tl = m.cells[0];
        for (Pos c : m.cells) tl = {std::min(tl.x, c.x), std::min(tl.y, c.y)};
        const Rectangle r = cellRect(tl);
        const float bob = std::sin(time_ * 3) > 0 ? 0 : scale_;
        atlas().draw(monsterSprite(m.kind), r.x, r.y + bob, monsterColor(m.kind), scale_ * 2);
        for (int i = 0; i < m.hp; ++i)
            DrawRectangle(int(r.x + 2 + float(i) * 5 * scale_), int(r.y + r.height * 2 - 3 * scale_), int(4 * scale_),
                          int(2 * scale_), pal::Red);
        if (monsterDef(m.kind).boss)
            for (int i = 0; i < enc_.bossArmor; ++i)
                DrawRectangle(int(r.x + 2 + float(i) * 5 * scale_), int(r.y + 1), int(4 * scale_), int(3 * scale_),
                              pal::Blue);
    }

    // Flare readouts
    for (int y : enc_.flareRows) {
        const Rectangle r = cellRect({0, y});
        ui::text(std::to_string(rowThreat(b, y)), r.x - 13, r.y + r.height / 2 - 5, pal::Orange);
    }
    for (int x : enc_.flareCols) {
        const Rectangle r = cellRect({x, 0});
        ui::textCentered(std::to_string(colThreat(b, x)), r.x + r.width / 2, r.y - 12, pal::Orange);
    }

    // Hover / targeting outline; numbers also outline the tiles they count.
    if (hasHover && !enc_.lost && !enc_.won) {
        if (isTargeted(pl, selectedSlot_)) {
            const ItemId id = pl.items[size_t(selectedSlot_)];
            if (id == ItemId::Scanner) {
                for (Pos q : b.within(hover, 1)) DrawRectangleLinesEx(cellRect(q), 1, pal::Blue);
            } else if (id == ItemId::Flare) {
                const Rectangle row = cellRect({0, hover.y});
                const Rectangle col = cellRect({hover.x, 0});
                DrawRectangleLinesEx({row.x, row.y, row.width * float(b.width()), row.height}, 1, pal::Orange);
                DrawRectangleLinesEx({col.x, col.y, col.width, col.height * float(b.height())}, 1, pal::Orange);
            }
            DrawRectangleLinesEx(cellRect(hover), 1, pal::Gold);
        } else {
            if (enc_.generated && b.at(hover).revealed)
                b.forNeighbors(hover, [&](Pos n) { DrawRectangleLinesEx(cellRect(n), 1, Fade(pal::Bone, 0.35f)); });
            DrawRectangleLinesEx(cellRect(hover), 1, pal::Bone);
        }
    }
    for (const auto& [p, t] : pulses_)
        if (b.inBounds(p)) DrawRectangleLinesEx(cellRect(p), 1, Fade(pal::Orange, 1.0f - t / 0.6f));

    for (const FloatText& f : floats_) {
        const float a = 1.0f - std::max(0.0f, f.age - 0.6f) / 0.5f;
        ui::textCentered(f.text, f.pos.x, f.pos.y - 6 - f.age * 18, Fade(f.color, a));
    }
    if (flash_ > 0) DrawRectangle(0, 0, ui::kCanvasW, ui::kCanvasH, Fade(pal::Red, flash_ * 0.35f));

    drawHoverInfo(pl);

    // HUD (drawn last so tooltips and clicks land on it)
    const auto hud = ui::playerPanel(run, &enc_, selectedSlot_);
    if (hud.discardSlot >= 0 && hud.discardSlot < int(pl.items.size()) && !enc_.lost && !enc_.won) {
        app.toast(std::string("Discarded the ") + itemDef(pl.items[size_t(hud.discardSlot)]).name + ".");
        discardItem(app.game->player, hud.discardSlot);
        selectedSlot_ = -1;
    }
    if (hud.clickedSlot >= 0 && !enc_.lost && !enc_.won) {
        if (isTargeted(pl, hud.clickedSlot)) selectedSlot_ = selectedSlot_ == hud.clickedSlot ? -1 : hud.clickedSlot;
        else act(app, {ActionType::UseItem, {}, Mark::None, hud.clickedSlot});
    }
    if (enc_.exitFound && !enc_.lost)
        if (ui::button({kAreaX + kAreaW - 84, 4, 84, 15}, "Descend")) confirmExit_ = true;

    // Modals
    auto modal = [&](const std::string& title, const std::string& body) {
        DrawRectangle(0, 0, ui::kCanvasW, ui::kCanvasH, Fade(BLACK, 0.55f));
        const Rectangle r{190, 112, 360, 116};
        ui::panel(r, pal::Bg, pal::Bone);
        ui::textCentered(ui::fitted(title, r.width - 20, 20), r.x + r.width / 2, r.y + 10, pal::Gold, 20);
        ui::textWrapped(body, r.x + 14, r.y + 36, r.width - 28, pal::Bone, 4);
        return r;
    };
    if (confirmExit_) {
        const Rectangle r = modal("Descend?", enc_.cleared ? "The board is clear. Onward."
                                                           : "Anything still hidden here stays behind.");
        if (ui::button({r.x + 50, r.y + 84, 110, 22}, "Descend") || IsKeyPressed(KEY_ENTER)) {
            confirmExit_ = false;
            act(app, {ActionType::Descend});
        } else if (ui::button({r.x + 200, r.y + 84, 110, 22}, "Stay") || IsKeyPressed(KEY_ESCAPE)) {
            confirmExit_ = false;
        }
    } else if (enc_.lost) {
        const Rectangle r = modal("You have fallen", "The descent ends here. Your story will be told.");
        if (ui::button({r.x + 125, r.y + 84, 110, 22}, "Results")) {
            finishBoard(run, enc_);
            app.toEnd();
        }
    } else if (enc_.won) {
        const bool last = run.act >= kActs;
        const Rectangle r = modal(last ? "Victory!" : "Act " + std::to_string(run.act) + " cleared!",
                                  bossName(run.map.bossKind) +
                                      (last ? " is slain and the depths fall silent." : " is slain. Deeper still..."));
        if (ui::button({r.x + 125, r.y + 84, 110, 22}, last ? "Results" : "Claim reward")) {
            finishBoard(run, enc_);
            if (run.over) app.toEnd();
            else app.go(std::make_unique<BossRewardScene>());
        }
    } else if (!popupTitle_.empty()) {
        const Rectangle r = modal(popupTitle_, popupBody_);
        if (ui::button({r.x + 125, r.y + 84, 110, 22}, "Got it") || IsKeyPressed(KEY_ENTER)) popupTitle_.clear();
    }
}

} // namespace flagd
