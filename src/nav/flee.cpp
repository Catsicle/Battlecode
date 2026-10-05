#include "grid.hpp"

namespace bot::nav {

extern int reachable_area(const Grid&, int, int);

Dir safest_move(const Grid& grid, Vec2 position, bool& found) {
    found = false;
    int best_area = -1;
    Dir best = Dir::NORTH;
    int cell = grid.memory.index(position);
    for (Dir direction : directions()) {
        int d = dir_index(direction);
        if (!grid.passable_edge(cell, d))
            continue;
        int next = grid.memory.index(add(position, direction, grid.memory.width, grid.memory.height));
        if (!grid.known(next) || grid.memory.occupancy[next].present)
            continue;
        int area = reachable_area(grid, next, 0);
        if (!found || area > best_area) {
            found = true;
            best_area = area;
            best = direction;
        }
    }
    return best;
}

} // namespace bot::nav
