#include "core/map.h"

#include <algorithm>
#include <cstdlib>

#include "core/content/balance.h"
#include "core/rng.h"

namespace flagd {

const char* nodeTypeName(NodeType t) {
    switch (t) {
        case NodeType::Normal: return "Board";
        case NodeType::Elite: return "Elite Board";
        case NodeType::Shop: return "Shop";
        case NodeType::Rest: return "Rest Site";
        case NodeType::Treasure: return "Treasure";
        case NodeType::Boss: return "Boss";
    }
    return "?";
}

bool isBoardNode(NodeType t) {
    return t == NodeType::Normal || t == NodeType::Elite || t == NodeType::Boss;
}

std::vector<int> Map::layer(int l) const {
    std::vector<int> ids;
    for (int i = 0; i < int(nodes.size()); ++i)
        if (nodes[size_t(i)].layer == l) ids.push_back(i);
    return ids;
}

int Map::bossId() const {
    for (int i = 0; i < int(nodes.size()); ++i)
        if (nodes[size_t(i)].type == NodeType::Boss) return i;
    return -1;
}

namespace {

NodeType rollType(int layer, Rng& rng) {
    struct Weight { NodeType t; int w; int minLayer; };
    static constexpr Weight kWeights[] = {
        {NodeType::Normal, 46, 0},
        {NodeType::Elite, 16, 2},
        {NodeType::Shop, 14, 1},
        {NodeType::Rest, 10, 3},
        {NodeType::Treasure, 14, 1},
    };
    int total = 0;
    for (const auto& w : kWeights)
        if (layer >= w.minLayer) total += w.w;
    int roll = rng.range(0, total - 1);
    for (const auto& w : kWeights) {
        if (layer < w.minLayer) continue;
        if (roll < w.w) return w.t;
        roll -= w.w;
    }
    return NodeType::Normal;
}

void link(Map& m, int from, int to) {
    auto& next = m.nodes[size_t(from)].next;
    if (std::find(next.begin(), next.end(), to) == next.end()) next.push_back(to);
}

} // namespace

Map generateMap(uint64_t seed) {
    Rng rng(deriveSeed(seed, 0x3A9));
    Map m;
    m.layers = balance::kMapLayers;
    const int bossLayer = m.layers - 1;

    for (int l = 0; l < bossLayer; ++l) {
        std::vector<int> cols{0, 1, 2, 3};
        rng.shuffle(cols);
        const int count = l == 0 ? 3 : rng.range(2, 4);
        cols.resize(size_t(count));
        std::sort(cols.begin(), cols.end());
        bool hasBoard = false;
        for (int c : cols) {
            MapNode n;
            n.layer = l;
            n.col = c;
            if (l == 0) n.type = NodeType::Normal;
            else if (l == bossLayer - 1) n.type = NodeType::Rest;  // breather before the boss
            else n.type = rollType(l, rng);
            hasBoard = hasBoard || isBoardNode(n.type);
            m.nodes.push_back(n);
        }
        // Every non-rest layer offers at least one board, so XP stays available.
        if (!hasBoard && l != bossLayer - 1) m.nodes.back().type = NodeType::Normal;
    }
    MapNode boss;
    boss.layer = bossLayer;
    boss.col = 1;  // drawn centred
    boss.type = NodeType::Boss;
    m.nodes.push_back(boss);

    for (int l = 0; l < bossLayer; ++l) {
        const std::vector<int> cur = m.layer(l);
        const std::vector<int> nxt = m.layer(l + 1);
        if (l + 1 == bossLayer) {
            for (int a : cur) link(m, a, nxt[0]);
            continue;
        }
        auto dist = [&](int a, int b) { return std::abs(m.nodes[size_t(a)].col - m.nodes[size_t(b)].col); };
        for (int a : cur) {
            std::vector<int> byDist = nxt;
            std::stable_sort(byDist.begin(), byDist.end(), [&](int x, int y) { return dist(a, x) < dist(a, y); });
            link(m, a, byDist[0]);
            if (byDist.size() > 1 && dist(a, byDist[1]) <= 1 && rng.chance(0.55)) link(m, a, byDist[1]);
        }
        for (int b : nxt) {
            bool reached = false;
            for (int a : cur) {
                const auto& next = m.nodes[size_t(a)].next;
                reached = reached || std::find(next.begin(), next.end(), b) != next.end();
            }
            if (reached) continue;
            int best = cur[0];
            for (int a : cur)
                if (dist(a, b) < dist(best, b)) best = a;
            link(m, best, b);
        }
    }
    return m;
}

} // namespace flagd
