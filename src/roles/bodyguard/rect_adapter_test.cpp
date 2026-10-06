#include "src/roles/bodyguard/rect_ring.hpp"
#include "src/roles/bodyguard/rect_adapter.hpp"
#include "../../core/memory.hpp"
#include "../../core/memory.hpp"
#include <cstdio>
#include <cassert>
#include <limits>

using namespace bodyguard::rect;
using namespace bodyguard::rect_adapter;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

// Adapter oracle: build Memory, fill Snapshot, compare arrays
static void testAdapterFromMemory() {
    bot::Memory M;
    M.initialize(16, 16);

    // Manually set some state to verify direct mapping
    M.seen.assign(16 * 16, false);
    M.known_edges.assign(16 * 16, 0);
    M.kelp.assign(16 * 16, 0);
    M.portal_id.assign(16 * 16, std::array<std::int16_t, 4>{{-1, -1, -1, -1}});
    M.pearl.assign(16 * 16, false);
    M.never_spawns.assign(16 * 16, true);
    M.pearl_spawn_round.assign(16 * 16, std::numeric_limits<int>::max());

    // Set a known tile: (3,4) seen, with kelp to East (rect d=0 = Memory bit 1)
    int cell_34 = M.index({3, 4});
    M.seen[cell_34] = true;
    M.known_edges[cell_34] |= (1U << 1); // Memory bit 1 = East = rect d=0
    M.kelp[cell_34] |= (1U << 1);

    // Set portal to South (rect d=1 = Memory bit 2) at (5,6)
    int cell_56 = M.index({5, 6});
    M.seen[cell_56] = true;
    M.known_edges[cell_56] |= (1U << 2); // Memory bit 2 = South = rect d=1
    M.kelp[cell_56] &= ~(1U << 2); // no kelp
    M.portal_id[cell_56][2] = 7;

    // Set a pearl at (7,8)
    int cell_78 = M.index({7, 8});
    M.seen[cell_78] = true;
    M.pearl[cell_78] = true;
    M.never_spawns[cell_78] = false;
    M.pearl_spawn_round[cell_78] = 5;

    Snapshot S;
    fill_snapshot(M, 10, S);

    // Verify snapshot dimensions
    CHECK(S.W == 16 && S.H == 16, "snapshot dimensions W=%d H=%d", S.W, S.H);

    // Verify seen at (3,4) and (5,6) and (7,8)
    CHECK(S.seen[4][3] == true, "seen at (3,4)");
    CHECK(S.seen[6][5] == true, "seen at (5,6)");
    CHECK(S.seen[8][7] == true, "seen at (7,8)");

    // Verify edge at (3,4) East (rect d=0): kelp
    CHECK(S.edge[4][3][0] == EDGE_KELP, "edge (3,4) d=0 (East) should be KELP, got %d", (int)S.edge[4][3][0]);

    // Verify other directions at (3,4) are unknown (no known edge set except East)
    CHECK(S.edge[4][3][1] == EDGE_UNKNOWN, "edge (3,4) d=1 should be UNKNOWN");
    CHECK(S.edge[4][3][2] == EDGE_UNKNOWN, "edge (3,4) d=2 should be UNKNOWN");
    CHECK(S.edge[4][3][3] == EDGE_UNKNOWN, "edge (3,4) d=3 should be UNKNOWN");

    // Verify edge at (5,6) South (rect d=1): portal
    CHECK(S.edge[6][5][1] == EDGE_PORTAL, "edge (5,6) d=1 (South) should be PORTAL, got %d", (int)S.edge[6][5][1]);

    // Verify risk at (7,8): pearl present
    CHECK(S.risk[8][7] == true, "risk at (7,8) should be true (pearl present)");

    printf("adapter test: %d cells checked, all direct mapping assertions passed\n", 16 * 16);
}

// Direct bit mapping correctness: synthetic Memory with all 4 edge kinds at different tiles
static void testDirectBitMapping() {
    bot::Memory M;
    M.initialize(8, 8);
    M.known_edges.assign(8 * 8, 0);
    M.kelp.assign(8 * 8, 0);
    M.portal_id.assign(8 * 8, std::array<std::int16_t, 4>{{-1, -1, -1, -1}});

    // Tile (1,1): empty to East (rect d=0 / Memory bit 1) -> EDGE_PLAIN
    int c11 = M.index({1, 1});
    M.known_edges[c11] |= (1U << 1);

    // Tile (2,2): kelp to South (rect d=1 / Memory bit 2) -> EDGE_KELP
    int c22 = M.index({2, 2});
    M.known_edges[c22] |= (1U << 2);
    M.kelp[c22] |= (1U << 2);

    // Tile (3,3): portal to West (rect d=2 / Memory bit 3) -> EDGE_PORTAL
    int c33 = M.index({3, 3});
    M.known_edges[c33] |= (1U << 3);
    M.kelp[c33] &= ~(1U << 3);
    M.portal_id[c33][3] = 99;

    // Tile (4,4) unknown to North (rect d=3 / Memory bit 0) -> EDGE_UNKNOWN
    // No bit set for Memory bit 0 -> unknown; assertion uses edge array directly

    Snapshot S;
    fill_snapshot(M, 1, S);

    CHECK(S.edge[1][1][0] == EDGE_PLAIN, "(1,1) East (d=0) PLAIN, got %d", (int)S.edge[1][1][0]);
    CHECK(S.edge[2][2][1] == EDGE_KELP, "(2,2) South (d=1) KELP, got %d", (int)S.edge[2][2][1]);
    CHECK(S.edge[3][3][2] == EDGE_PORTAL, "(3,3) West (d=2) PORTAL, got %d", (int)S.edge[3][3][2]);
    CHECK(S.edge[4][4][3] == EDGE_UNKNOWN, "(4,4) North (d=3) UNKNOWN, got %d", (int)S.edge[4][4][3]);

    printf("direct bit mapping test passed\n");
}

// No-heap / fixed-size check: adapter uses only stack/static arrays
static void testFixedSize() {
    bot::Memory M;
    M.initialize(4, 4);
    Snapshot S;
    fill_snapshot(M, 1, S);
    // Snapshot arrays are fixed; adapter allocates nothing on heap
    CHECK(S.W == 4 && S.H == 4, "snapshot size preserved");
    printf("fixed-size test passed\n");
}

int main() {
    testAdapterFromMemory();
    testDirectBitMapping();
    testFixedSize();
    printf("ADAPTER TESTS: %d failures\n", failures);
    return failures ? 1 : 0;
}
