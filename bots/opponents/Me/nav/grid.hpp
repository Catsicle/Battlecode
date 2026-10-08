#pragma once

#include "../core/memory.hpp"

namespace bot::nav {

struct Grid {
    const Memory& memory;
    int round = 0;

    bool known(int cell) const { return memory.known(cell); }
    bool frontier(int cell) const { return memory.is_frontier(cell); }
    bool has_pearl(int cell) const { return memory.pearl[cell]; }
    bool passable_edge(int cell, int direction) const {
        return memory.edge_known(cell, direction) && (memory.kelp[cell] & (1U << direction)) == 0;
    }
};

} // namespace bot::nav
