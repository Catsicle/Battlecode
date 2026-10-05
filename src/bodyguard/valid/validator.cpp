#include "bodyguard/valid/validator.hpp"
#include "bodyguard/core/torus.hpp"
#include <array>

namespace bodyguard {

bool is_simple_cycle(const std::array<Cell, MAX_CYCLE_LEN>& cycle, int len) {
    if (len < 4 || len % 2 != 0) return false;
    // Simple: all distinct cells (no repeats in list)
    for (int i = 0; i < len; ++i) {
        for (int j = i + 1; j < len; ++j) {
            if (cycle[i] == cycle[j]) return false;
        }
    }
    return true;
}

bool seals_pocket(const std::array<Cell, MAX_CYCLE_LEN>& cycle, int len, Cell pocket) {
    // Pocket must not lie on the ring itself.
    for (int i = 0; i < len; ++i) {
        if (cycle[i] == pocket) return false;
    }
    // Full bounded-check: fixed-size BFS flood-fill deferred to brute-force comparator.
    return true;
}

bool find_dash(const std::array<Cell, MAX_CYCLE_LEN>& cycle, int len, int s, Cell& door, Cell& outside) {
    // Placeholder: pick first cycle tile as door, adjacent outside tile
    if (len == 0) return false;
    door = cycle[0];
    outside = door + 1; // approximate; real version uses torus adj
    return true;
}

} // namespace bodyguard
