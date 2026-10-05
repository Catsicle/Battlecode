#include "grid.hpp"
#include "../config.hpp"

#include <limits>

namespace bot::nav {

extern Path search(const Grid&, Vec2, Vec2, bool, int, int = -1);

Path approach_target(const Grid& grid, Vec2 start, Vec2 target, int length) {
    if (start == target)
        return {};

    int target_cell = grid.memory.index(target);
    if (grid.known(target_cell)) {
        Path direct = search(grid, start, target, true, length);
        if (direct.found)
            return direct;
    }

    Path best;
    int best_cost = std::numeric_limits<int>::max();
    for (int cell = 0; cell < grid.memory.cells; ++cell) {
        if (!grid.frontier(cell))
            continue;
        Path candidate = search(grid, start, grid.memory.position(cell), true, length);
        if (!candidate.found || candidate.length == 0)
            continue;
        if (candidate.cost < best_cost) {
            best = candidate;
            best_cost = candidate.cost;
        }
    }
    return best;
}

Path wander(const Grid& grid, Vec2 start, int length) {
    int start_cell = grid.memory.index(start);
    int best_cell = -1;
    int best_score = std::numeric_limits<int>::min();
    for (int cell = 0; cell < grid.memory.cells; ++cell) {
        if (!grid.known(cell))
            continue;
        int score = (grid.memory.pearl[cell] ? B_PEARL : 0)
                  + (grid.frontier(cell) ? B_FRONT : 0)
                  + std::min(200, std::max(0, grid.round - grid.memory.last_seen_round[cell])) * B_STALE / 200
                  - grid.memory.visits_nopearl[cell] * LAMBDA;
        if (score > best_score) {
            best_score = score;
            best_cell = cell;
        }
    }
    if (best_cell < 0)
        best_cell = start_cell;
    return search(grid, start, grid.memory.position(best_cell), false, length);
}

} // namespace bot::nav
