#include "grid.hpp"
#include "../config.hpp"

#include <array>
#include <limits>
#include <queue>
#include <vector>

namespace bot::nav {

struct QueueNode {
    int cost;
    int cell;
    bool operator>(const QueueNode& other) const { return cost > other.cost; }
};

Path search(const Grid& grid, Vec2 start_position, Vec2 target_position, bool navigate,
            int length, int forbidden_first_direction) {
    Path result;
    int start = grid.memory.index(start_position);
    int target = grid.memory.index(target_position);
    if (start < 0 || target < 0 || !grid.known(start))
        return result;
    const int n = grid.memory.cells;
    std::vector<int> distance(n, std::numeric_limits<int>::max());
    std::vector<int> parent(n, -1);
    std::vector<int> parent_dir(n, -1);
    std::priority_queue<QueueNode, std::vector<QueueNode>, std::greater<QueueNode>> queue;
    distance[start] = 0;
    queue.push({0, start});

    while (!queue.empty()) {
        QueueNode current = queue.top();
        queue.pop();
        if (current.cost != distance[current.cell])
            continue;
        if (current.cell == target)
            break;
        Vec2 p = grid.memory.position(current.cell);
        for (Dir direction : directions()) {
            int d = dir_index(direction);
            if (current.cell == start && d == forbidden_first_direction)
                continue;
            if (!grid.passable_edge(current.cell, d))
                continue;
            Vec2 next_position = add(p, direction, grid.memory.width, grid.memory.height);
            int next = grid.memory.index(next_position);
            if (!grid.known(next))
                continue;
            // The route is committed one step at a time, so no currently
            // occupied tile is a safe destination, including the own body.
            if (grid.memory.occupancy[next].present)
                continue;
            int step_cost = C_STEP;
            if (!grid.memory.pearl[next] && !grid.memory.eaten_here[next]) {
                int age = grid.round - grid.memory.last_visit_round[next];
                age = age < 0 ? 0 : age;
                step_cost += std::min(P_MAX, LAMBDA * grid.memory.visits_nopearl[next] * AGE_K / (AGE_K + age));
            }
            if (navigate)
                step_cost = C_STEP;
            int candidate = current.cost + step_cost;
            if (candidate >= distance[next])
                continue;
            distance[next] = candidate;
            parent[next] = current.cell;
            parent_dir[next] = d;
            queue.push({candidate, next});
        }
    }
    if (distance[target] == std::numeric_limits<int>::max())
        return result;
    std::vector<Dir> reversed;
    for (int cell = target; cell != start && cell >= 0 && reversed.size() < 256; cell = parent[cell]) {
        reversed.push_back(directions()[parent_dir[cell]]);
    }
    int count = static_cast<int>(reversed.size());
    if (count == 0 && start != target)
        return result;
    for (int i = 0; i < count; ++i)
        result.steps.push_back(reversed[count - i - 1]);
    result.length = count;
    result.cost = distance[target];
    result.found = true;
    (void)length;
    return result;
}

} // namespace bot::nav
