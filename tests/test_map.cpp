#include <doctest/doctest.h>

#include <set>

#include "core/content/balance.h"
#include "core/map.h"

using namespace flagd;

TEST_CASE("map layers: boards first, rest before boss, every node reachable") {
    for (uint64_t seed = 1; seed <= 200; ++seed) {
        const Map m = generateMap(seed);
        REQUIRE(m.layers == balance::kMapLayers);
        for (int id : m.layer(0)) CHECK(m.nodes[size_t(id)].type == NodeType::Normal);
        for (int id : m.layer(m.layers - 2)) CHECK(m.nodes[size_t(id)].type == NodeType::Rest);
        REQUIRE(m.layer(m.layers - 1).size() == 1);
        CHECK(m.nodes[size_t(m.bossId())].type == NodeType::Boss);

        std::set<int> reached;
        for (int id : m.layer(0)) reached.insert(id);
        for (int l = 0; l < m.layers - 1; ++l)
            for (int id : m.layer(l)) {
                CHECK(reached.count(id) == 1);
                CHECK_FALSE(m.nodes[size_t(id)].next.empty());
                for (int n : m.nodes[size_t(id)].next) {
                    CHECK(m.nodes[size_t(n)].layer == l + 1);
                    reached.insert(n);
                }
            }
        CHECK(reached.count(m.bossId()) == 1);
    }
}
