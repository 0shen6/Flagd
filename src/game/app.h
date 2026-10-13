#pragma once
#include <raylib.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/meta.h"
#include "core/run.h"

namespace flagd {

class App;

class Scene {
public:
    virtual ~Scene() = default;
    virtual void update(App& app, float dt) = 0;
    virtual void draw(App& app) = 0;
    // Run screens can be paused with Esc (the pause menu holds the options).
    virtual bool pausable() const { return false; }
    // True while the scene itself uses Esc (e.g. to cancel an item).
    virtual bool wantsEscape() const { return false; }
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
    // Screenshot mode also keeps its save file in `dir`, away from the real one.
    void setShotScript(std::string dir, std::vector<ShotStep> steps) {
        shotDir_ = std::move(dir);
        shotSteps_ = std::move(steps);
    }

    // Scene changes take effect at the start of the next frame.
    void go(std::unique_ptr<Scene> next) { pending_ = std::move(next); }
    void toTitle();
    void toMap();
    void toEnd();  // banks the run into meta progress (once) and saves
    void startRun(uint64_t seed, ClassId cls, int heat);
    void enterNode(int nodeId);
    void saveProgress();

    void shake(float seconds) { shake_ = std::max(shake_, seconds); }
    // A short notice at the bottom of the screen (item use outside boards etc.).
    void toast(std::string text) {
        toast_ = std::move(text);
        toastAge_ = 0;
    }
    // Handles item clicks from the side panel on screens without a board.
    void handleHudItems(int clickedSlot, int discardSlot);
    void quit() { quit_ = true; }
    void setPaused(bool on) { paused_ = on; }

    // Settings (saved with progress). Window changes are applied at the start
    // of the next frame, never in the middle of drawing.
    void setFullscreen(bool on);
    bool fullscreen() const { return fullscreen_; }
    void setMuted(bool on);
    void setWindowPreset(int index);
    void setPixelPerfect(bool on);
    // Draws the option toggles at (x, y); returns the height used.
    float drawOptions(float x, float y);

    std::optional<Run> game;
    Meta meta;
    RunResult lastResult;  // embers and unlocks from the run that just ended

private:
    void present();

    std::unique_ptr<Scene> scene_;
    std::unique_ptr<Scene> pending_;
    RenderTexture2D canvas_{};
    float shake_ = 0;
    bool quit_ = false;
    void loadProgress();
    std::string savePath() const;

    void drawPause();
    void applyWindowRequests();
    void enterFullscreen();
    void leaveFullscreen();

    std::string toast_;
    float toastAge_ = 99;
    bool fullscreen_ = false;
    int wantFullscreen_ = -1;  // -1 none, else 0/1
    int wantPreset_ = -1;
    Vector2 savedPos_{};
    int savedW_ = 0;
    int savedH_ = 0;

    bool paused_ = false;
    bool runBanked_ = false;
    std::string shotDir_;
    std::vector<ShotStep> shotSteps_;
};

} // namespace flagd
