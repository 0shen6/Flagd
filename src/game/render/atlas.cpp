#include "game/render/atlas.h"

#include "game/render/ui.h"

namespace flagd {

Spr monsterSprite(MonsterKind k) {
    switch (k) {
        case MonsterKind::Slime: return {20, 5};
        case MonsterKind::Bat: return {26, 8};
        case MonsterKind::Skeleton: return {29, 6};
        case MonsterKind::Spider: return {28, 5};
        case MonsterKind::Burrower: return {31, 8};
        case MonsterKind::Count: break;
    }
    return spr::Skull;
}

Color monsterColor(MonsterKind k) {
    switch (k) {
        case MonsterKind::Slime: return pal::Green;
        case MonsterKind::Bat: return pal::Blue;
        case MonsterKind::Skeleton: return pal::Bone;
        case MonsterKind::Spider: return pal::Red;
        case MonsterKind::Burrower: return pal::Gold;
        case MonsterKind::Count: break;
    }
    return WHITE;
}

Spr itemSprite(ItemId id) {
    switch (id) {
        case ItemId::Scanner: return {37, 11};
        case ItemId::Defuser: return {29, 11};
        case ItemId::Flare: return {28, 11};
        case ItemId::Shield: return {37, 3};
        case ItemId::Potion: return {33, 13};
        case ItemId::Count: break;
    }
    return spr::Skull;
}

Spr relicSprite(RelicId id) {
    switch (id) {
        case RelicId::LuckyStart: return {43, 13};
        case RelicId::TenthStep: return {39, 1};
        case RelicId::BountyFlags: return {41, 4};
        case RelicId::ChordMiner: return {43, 5};
        case RelicId::WideChord: return {44, 6};
        case RelicId::ThickSkin: return {37, 1};
        case RelicId::CoinMagnet: return {44, 12};
        case RelicId::Count: break;
    }
    return spr::Skull;
}

Spr nodeSprite(NodeType t) {
    switch (t) {
        case NodeType::Normal: return {30, 11};
        case NodeType::Elite: return {38, 11};
        case NodeType::Shop: return {35, 16};
        case NodeType::Rest: return {14, 10};
        case NodeType::Treasure: return {8, 6};
        case NodeType::Boss: return {31, 8};
    }
    return spr::Skull;
}

Color nodeColor(NodeType t) {
    switch (t) {
        case NodeType::Normal: return pal::Bone;
        case NodeType::Elite: return pal::Red;
        case NodeType::Shop: return pal::Gold;
        case NodeType::Rest: return pal::Orange;
        case NodeType::Treasure: return pal::Brown;
        case NodeType::Boss: return pal::Gold;
    }
    return WHITE;
}

Color threatColor(int threat) {
    if (threat <= 2) return pal::Blue;
    if (threat <= 4) return pal::Green;
    if (threat <= 6) return pal::Gold;
    if (threat <= 9) return pal::Orange;
    return pal::Red;
}

bool Atlas::load(const char* path) {
    tex_ = LoadTexture(path);
    if (tex_.id == 0) return false;
    SetTextureFilter(tex_, TEXTURE_FILTER_POINT);
    return true;
}

void Atlas::unload() {
    if (tex_.id != 0) UnloadTexture(tex_);
    tex_ = {};
}

void Atlas::draw(Spr s, float x, float y, Color tint, float scale) const {
    if (tex_.id == 0) {
        // Missing art: draw a placeholder so the game stays playable.
        DrawRectangleLinesEx({x + 2 * scale, y + 2 * scale, 12 * scale, 12 * scale}, scale, tint);
        return;
    }
    const Rectangle src{float(s.col * 16), float(s.row * 16), 16, 16};
    const Rectangle dst{x, y, 16 * scale, 16 * scale};
    DrawTexturePro(tex_, src, dst, {0, 0}, 0, tint);
}

Atlas& atlas() {
    static Atlas instance;
    return instance;
}

} // namespace flagd
