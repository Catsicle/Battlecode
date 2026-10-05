#include "grid.hpp"

#include <array>
#include <queue>

namespace bot::nav {

int reachable_area(const Grid& grid, int start, int body_size) {
    if (!grid.known(start))
        return 0;
    std::vector<bool> visited(grid.memory.cells, false);
    std::queue<int> queue;
    queue.push(start);
    visited[start] = true;
    int count = 0;
    while (!queue.empty()) {
        int current = queue.front();
        queue.pop();
        ++count;
        Vec2 p = grid.memory.position(current);
        for (Dir direction : directions()) {
            int d = dir_index(direction);
            if (!grid.passable_edge(current, d))
                continue;
            int next = grid.memory.index(add(p, direction, grid.memory.width, grid.memory.height));
            if (!grid.known(next) || visited[next])
                continue;
            if (grid.memory.occupancy[next].present && next != start)
                continue;
            visited[next] = true;
            queue.push(next);
        }
    }
    (void)body_size;
    return count;
}

} // namespace bot::nav
