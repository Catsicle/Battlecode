#pragma once

#include "rect_ring.hpp"
#include "../../core/memory.hpp"

namespace bodyguard::rect_adapter {

// Direct mapping: rect_ring direction index -> Memory bit position
// rect_ring: 0=East (+x), 1=South (+y), 2=West (-x), 3=North (-y)
// Memory (memory.cpp): bit positions 0=North, 1=East, 2=South, 3=West
constexpr int RECT_D_TO_MEM_BIT[4] = {1, 2, 3, 0}; // E->1, S->2, W->3, N->0

inline void fill_snapshot(const bot::Memory& memory, int current_round,
                          bodyguard::rect::Snapshot& s) {
    int W = memory.width;
    int H = memory.height;

    // rect_ring.hpp array limits: MAX_W=64, MAX_H=64
    if (W <= 0 || H <= 0 || W > 64 || H > 64) {
        s.reset(0, 0);
        return;
    }

    s.reset(W, H);

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            int cell = memory.index({x, y});

            // SEEN: use Memory.seen directly; rect_ring convention needs no translation
            s.seen[y][x] = memory.seen[cell];

            // RISK: pearl present or about to spawn (use Memory fields directly)
            bool has_pearl_now = memory.pearl[cell];
            bool never = memory.never_spawns[cell];
            int spawn_round = memory.pearl_spawn_round[cell];
            s.risk[y][x] = has_pearl_now || (!never && spawn_round != std::numeric_limits<int>::max() && spawn_round <= current_round + 48);

            // EDGE: read Memory bit arrays directly using rect_ring d mapped to Memory bit index
            for (int d = 0; d < 4; ++d) {
                int mem_bit = RECT_D_TO_MEM_BIT[d];
                bool known = (memory.known_edges[cell] >> mem_bit) & 1;

                if (!known) {
                    s.edge[y][x][d] = bodyguard::rect::EDGE_UNKNOWN;
                } else if ((memory.kelp[cell] >> mem_bit) & 1) {
                    s.edge[y][x][d] = bodyguard::rect::EDGE_KELP;
                } else if (memory.portal_id[cell][mem_bit] >= 0) {
                    s.edge[y][x][d] = bodyguard::rect::EDGE_PORTAL;
                } else {
                    s.edge[y][x][d] = bodyguard::rect::EDGE_PLAIN;
                }
            }
        }
    }

    s.finalize();
}

// Query function: return the top k best Box structs for a pocket at (px,py).
// Writes up to k boxes into `out` (must have space for at least k entries).
// Returns the actual number written (<= k, <= 64). Uses rect_ring::generate().
inline int query_top_boxes(const bodyguard::rect::Snapshot& snapshot, int px, int py, int cap, int k, bodyguard::rect::Box* out) {
    if (k <= 0 || out == nullptr) return 0;
    if (k > 64) k = 64; // clamp to safe fixed-size cap
    return bodyguard::rect::generate(snapshot, px, py, cap, out, k);
}

} // namespace bodyguard::rect_adapter
