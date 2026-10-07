#include "planner.hpp"

#include "../config.hpp"

namespace bot::nav {

Path search(const Grid&, Vec2, Vec2, bool, int, int);
Path wander(const Grid&, Vec2, int, std::uint8_t);
Path approach_target(const Grid&, Vec2, Vec2, int);

Path plan(const roles::Action::PlanRequest& request, const Grid& grid,
          const Sim&, Vec2 start, std::uint8_t forbid_mask) {
    if (request.mode == NavMode::Navigate && request.has_target) {
        // 1. Direct A* search to target
        Path path = search(grid, start, request.target, true, 3, forbid_mask);
        if (path.found && path.length > 0)
            return path;

        // 2. Approach target through frontiers if destination is unknown/partially blocked
        path = approach_target(grid, start, request.target, 3);
        if (path.found && path.length > 0)
            return path;
    }
    
    // 3. Fallback to exploration only if navigation completely fails
    return wander(grid, start, 3, forbid_mask);
}

} // namespace bot::nav