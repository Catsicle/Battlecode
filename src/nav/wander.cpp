#include "grid.hpp"
#include "../config.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <vector>

namespace bot::nav {

extern Path search(const Grid&, Vec2, Vec2, bool, int, int = -1);

int toroidal_distance(Vec2 first, Vec2 second, int width, int height) {
    int dx = std::abs(first.x - second.x);
    int dy = std::abs(first.y - second.y);
    dx = std::min(dx, width - dx);
    dy = std::min(dy, height - dy);
    return dx + dy;
}

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
    int best_target_distance = std::numeric_limits<int>::max();
    int best_cost = std::numeric_limits<int>::max();
    int best_length = std::numeric_limits<int>::max();
    for (int cell = 0; cell < grid.memory.cells; ++cell) {
        if (!grid.frontier(cell))
            continue;
        Path candidate = search(grid, start, grid.memory.position(cell), true, length);
        if (!candidate.found || candidate.length == 0)
            continue;
        int target_distance = toroidal_distance(
            grid.memory.position(cell),
            target,
            grid.memory.width,
            grid.memory.height);
        bool better_progress = target_distance < best_target_distance;
        bool equal_progress = target_distance == best_target_distance;
        bool better_cost = candidate.cost < best_cost;
        bool equal_cost = candidate.cost == best_cost;
        if (better_progress ||
            (equal_progress && (better_cost ||
                                (equal_cost && candidate.length < best_length)))) {
            best = candidate;
            best_target_distance = target_distance;
            best_cost = candidate.cost;
            best_length = candidate.length;
        }
    }
    return best;
}

Path wander(const Grid& grid, Vec2 start, int length, std::uint8_t forbid_mask) {
    struct Candidate {
        int cell;
        int score;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(grid.memory.cells);

    for (int cell = 0; cell < grid.memory.cells; ++cell) {
        if (!grid.known(cell))
            continue;
        int score = (grid.memory.pearl[cell] ? B_PEARL : 0)
                  + (grid.frontier(cell) ? B_FRONT : 0)
                  + std::min(200, std::max(0, grid.round - grid.memory.last_seen_round[cell])) * B_STALE / 200
                  - grid.memory.visits_nopearl[cell] * LAMBDA;
        candidates.push_back({cell, score});
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.score > b.score;
    });

    for (const auto& cand : candidates) {
        Path path = search(grid, start, grid.memory.position(cand.cell), false, length, forbid_mask);
        if (path.found && path.length > 0)
            return path;
    }

    return {};
}

} // namespace bot::nav