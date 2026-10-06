#include "planner.hpp"

#include "../config.hpp"

namespace bot::nav {

Path search(const Grid&, Vec2, Vec2, bool, int, int);
Path wander(const Grid&, Vec2, int, std::uint8_t);

Path plan(const roles::Action::PlanRequest& request, const Grid& grid,
          const Sim&, Vec2 start, std::uint8_t forbid_mask) {
    if (request.mode == NavMode::Navigate && request.has_target) {
        Path path = search(grid, start, request.target, true, 3, forbid_mask);
        if (path.found && path.length > 0)
            return path;
    }
    return wander(grid, start, 3, forbid_mask);
}

} // namespace bot::nav