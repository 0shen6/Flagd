#pragma once
// Slay-the-Spire style layered map: pick one node per layer, boss at the end.
#include <cstdint>
#include <vector>

#include "core/content/content.h"

namespace flagd {

enum class NodeType : uint8_t { Normal, Elite, Shop, Rest, Treasure, Event, Boss };

const char* nodeTypeName(NodeType t);
bool isBoardNode(NodeType t);

struct MapNode {
    int layer = 0;
    int col = 0;  // 0..kMapColumns-1
    NodeType type = NodeType::Normal;
    Modifier modifier = Modifier::None;  // board nodes only
    std::vector<int> next;  // node ids in layer + 1
};

constexpr int kMapColumns = 4;

struct Map {
    std::vector<MapNode> nodes;
    int layers = 0;
    MonsterKind bossKind = MonsterKind::Burrower;

    std::vector<int> layer(int l) const;
    int bossId() const;
};

// Heat 4+ puts a modifier on every board.
Map generateMap(uint64_t seed, int heat = 0);

} // namespace flagd
