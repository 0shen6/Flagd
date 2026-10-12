#pragma once
#include <raylib.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/run.h"

namespace flagd {

class App;

class Scene {
public:
    virtual ~Scene() = default;
    virtual void update(App& app, float dt) = 0;
    virtual void draw(App& app) = 0;
};

// Developer aid: scripted frames that fake input and save canvas screenshots.
struct ShotStep {
    int frame;
    std::function<void(App&)> action;  // may fake ui::input() for this frame
    std::string shot;                  // file name to save after drawing, if set
};

class App {
public:
    int loop();
    void setShotScript(std::string dir, std::vector<ShotStep> steps) {
        shotDir_ = std::move(dir);
        shotSteps_ = std::move(steps);
    }

    // Scene changes take effect at the start of the next frame.
    void go(std::unique_ptr<Scene> next) { pending_ = std::move(next); }
    void toTitle();
    void toMap();
    void toEnd();
    void startRun(uint64_t seed);
    void enterNode(int nodeId);

    void shake(float seconds) { shake_ = std::max(shake_, seconds); }
    void quit() { quit_ = true; }

    std::optional<Run> game;

private:
    void present();

    std::unique_ptr<Scene> scene_;
    std::unique_ptr<Scene> pending_;
    RenderTexture2D canvas_{};
    float shake_ = 0;
    bool quit_ = false;
    std::string shotDir_;
    std::vector<ShotStep> shotSteps_;
};

} // namespace flagd
