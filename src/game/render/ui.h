#pragma once
// Immediate-mode UI helpers drawn on the 640x360 canvas.
#include <raylib.h>

#include <string>

#include "core/game_state.h"
#include "core/run.h"

namespace flagd {

namespace pal {
// Kenney 1-Bit Pack palette plus UI darks.
constexpr Color Bg{24, 22, 30, 255};
constexpr Color Panel{36, 33, 44, 255};
constexpr Color PanelHi{52, 48, 62, 255};
constexpr Color Line{74, 68, 88, 255};
constexpr Color Hidden{58, 54, 72, 255};
constexpr Color HiddenHi{78, 73, 96, 255};
constexpr Color Floor{30, 28, 38, 255};
constexpr Color Bone{207, 198, 184, 255};
constexpr Color Dim{128, 120, 136, 255};
constexpr Color Brown{191, 121, 88, 255};
constexpr Color Red{230, 72, 46, 255};
constexpr Color Blue{60, 172, 215, 255};
constexpr Color Green{56, 217, 115, 255};
constexpr Color Gold{244, 180, 27, 255};
constexpr Color Orange{238, 132, 52, 255};
constexpr Color Maroon{122, 68, 74, 255};
constexpr Color Purple{176, 98, 220, 255};
} // namespace pal

namespace ui {

constexpr int kCanvasW = 640;
constexpr int kCanvasH = 360;

struct Input {
    Vector2 mouse{};
    bool left = false;    // pressed this frame
    bool right = false;
    bool middle = false;
};

Input& input();
bool hover(Rectangle r);
bool clicked(Rectangle r);

void text(const std::string& s, float x, float y, Color c, int size = 10);
void textCentered(const std::string& s, float cx, float y, Color c, int size = 10);
int textWidth(const std::string& s, int size = 10);
// Word-wraps into maxWidth; returns height used.
int textWrapped(const std::string& s, float x, float y, float maxWidth, Color c);

void panel(Rectangle r, Color fill = pal::Panel, Color border = pal::Line);
void bar(Rectangle r, float frac, Color fill, Color back = pal::Floor);
bool button(Rectangle r, const std::string& label, bool enabled = true);

// Tooltip shown at the end of the frame next to the mouse.
void setTooltip(const std::string& title, const std::string& body);
void drawTooltip();

// Left-hand panel used by the map and board screens. Returns the y below it.
// enc may be null outside boards. selectedSlot highlights an item; clicking a
// slot sets clickedSlot.
struct HudResult {
    int clickedSlot = -1;
    float bottom = 0;  // y just below the items
};
HudResult playerPanel(const Run& run, const Encounter* enc, int selectedSlot);

std::string floorLabel(const Run& run, int nodeId);

} // namespace ui
} // namespace flagd
