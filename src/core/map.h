#pragma once
// Slay-the-Spire style layered map: pick one node per layer, boss at the end.
#include <cstdint>
#include <vector>

namespace flagd {

enum class NodeType : uint8_t { Normal, Elite, Shop, Rest, Treasure, Boss };

const char* nodeTypeName(NodeType t);
bool isBoardNode(NodeType t);

struct MapNode {
    int layer = 0;
    int col = 0;  // 0..kMapColumns-1
    NodeType type = NodeType::Normal;
    std::vector<int> next;  // node ids in layer + 1
};

constexpr int kMapColumns = 4;

struct Map {
    std::vector<MapNode> nodes;
    int layers = 0;

    std::vector<int> layer(int l) const;
    int bossId() const;
};

Map generateMap(uint64_t seed);

} // namespace flagd
