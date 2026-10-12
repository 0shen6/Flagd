#pragma once
// Kenney 1-Bit Pack sprites (monochrome, tinted at draw time).
#include <raylib.h>

#include "core/content/content.h"
#include "core/map.h"

namespace flagd {

struct Spr {
    int col;
    int row;
};

namespace spr {
constexpr Spr Digit(int d) { return {35 + d, 17}; }
constexpr Spr Flag{17, 9};
constexpr Spr Exit{2, 6};
constexpr Spr Chest{8, 6};
constexpr Spr Key{32, 11};
constexpr Spr Coin{41, 3};
constexpr Spr Heart{39, 10};
constexpr Spr Rubble{1, 11};
constexpr Spr Spark{36, 11};
constexpr Spr Helmet{32, 0};
constexpr Spr Trophy{40, 16};
constexpr Spr Skull{38, 11};
constexpr Spr Hourglass{40, 12};
constexpr Spr ShieldIcon{37, 3};
} // namespace spr

Spr monsterSprite(MonsterKind k);
Color monsterColor(MonsterKind k);
Spr itemSprite(ItemId id);
Spr relicSprite(RelicId id);
Spr nodeSprite(NodeType t);
Color nodeColor(NodeType t);
Color threatColor(int threat);

class Atlas {
public:
    bool load(const char* path);
    void unload();
    void draw(Spr s, float x, float y, Color tint, float scale = 1.0f) const;
    const Texture2D& texture() const { return tex_; }

private:
    Texture2D tex_{};
};

Atlas& atlas();

} // namespace flagd
