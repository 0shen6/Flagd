#pragma once
#include <string>
#include <vector>

#include "core/game_state.h"
#include "core/run.h"
#include "game/app.h"

namespace flagd {

class TitleScene : public Scene {
public:
    void update(App& app, float dt) override;
    void draw(App& app) override;

private:
    float time_ = 0;
};

class ClassSelectScene : public Scene {
public:
    explicit ClassSelectScene(App& app);
    void update(App& app, float dt) override;
    void draw(App& app) override;

private:
    ClassId cls_ = ClassId::Warrior;
    int heat_ = 0;
    std::string seedText_;
    float time_ = 0;
};

class CampScene : public Scene {
public:
    void update(App& app, float dt) override;
    void draw(App& app) override;

private:
    std::string note_;
};

class CodexScene : public Scene {
public:
    void update(App& app, float dt) override;
    void draw(App& app) override;

private:
    int tab_ = 0;
};

class MapScene : public Scene {
public:
    void update(App& app, float dt) override;
    void draw(App& app) override;

private:
    Vector2 nodePos(const Run& run, int id) const;
    float time_ = 0;
    int clickedNode_ = -1;
};

struct FloatText {
    Vector2 pos;
    std::string text;
    Color color;
    float age = 0;
};

class BoardScene : public Scene {
public:
    BoardScene(App& app, int nodeId);
    void update(App& app, float dt) override;
    void draw(App& app) override;

private:
    Rectangle cellRect(Pos p) const;
    bool cellAt(Vector2 m, Pos& out) const;
    Vector2 cellCenter(Pos p) const;
    void act(App& app, const Action& a);
    void react(App& app, const std::vector<Event>& events);
    void say(const std::string& msg, Color c);
    void floatAt(Pos p, const std::string& s, Color c);
    void drawCell(Pos p, bool hovered, bool seer);
    void drawHoverInfo();

    int nodeId_;
    Encounter enc_;
    float scale_ = 1;
    Vector2 origin_{};
    int selectedSlot_ = -1;
    bool confirmExit_ = false;
    float flash_ = 0;
    float time_ = 0;
    std::string log_;
    Color logColor_{};
    float logAge_ = 0;
    std::vector<FloatText> floats_;
    std::vector<std::pair<Pos, float>> pulses_;  // recently changed cells
};

class ShopScene : public Scene {
public:
    ShopScene(App& app, int nodeId);
    void update(App& app, float dt) override;
    void draw(App& app) override;

private:
    std::vector<ShopOffer> offers_;
    std::string note_;
};

class RestScene : public Scene {
public:
    void update(App& app, float dt) override;
    void draw(App& app) override;

private:
    bool done_ = false;
    std::string note_;
};

class TreasureScene : public Scene {
public:
    TreasureScene(App& app, int nodeId);
    void update(App& app, float dt) override;
    void draw(App& app) override;

private:
    TreasureLoot loot_;
};

class EventScene : public Scene {
public:
    explicit EventScene(int nodeId) : nodeId_(nodeId) {}
    void update(App& app, float dt) override;
    void draw(App& app) override;

private:
    int nodeId_;
    std::string outcome_;
};

class EndScene : public Scene {
public:
    void update(App& app, float dt) override;
    void draw(App& app) override;

private:
    float time_ = 0;
};

} // namespace flagd
