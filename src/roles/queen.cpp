#include "role.hpp"
#include "../config.hpp"
#include <array>
#include <utility>

namespace bot::roles {

static Vec2 wrap_offset(Vec2 base, int dx, int dy, int width, int height) {
    auto wrap = [](int val, int max_val) { return (val % max_val + max_val) % max_val; };
    return {wrap(base.x + dx, width), wrap(base.y + dy, height)};
}

class QueenRole final : public Role {
  private:
    bool initialized = false;
    Vec2 spawn_position{};
    std::size_t waypoint_index = 0;
    int turns_on_current_waypoint = 0;

  public:
    Action act(const unswbc::Controller& controller, const Memory& memory,
               const nav::Grid& grid, const comms::Inbox& inbox) override {
        (void)grid;
        Action action;
        Vec2 current_pos = controller.get_position();

        // 1. Save spawn location on first turn
        if (!initialized) {
            spawn_position = current_pos;
            initialized = true;
        }

        // 2. Target Received via Sonar / Inbox -> Navigate directly to Target
        // if (inbox.navigation_target.available) {
        //     action.request.mode = NavMode::Navigate;
        //     action.request.has_target = true;
        //     action.request.target = inbox.navigation_target.target;
        //     return action;
        // }

        // 3. Fixture override (if navigation testing fixture is enabled)
        if (NAVIGATION_FIXTURE && current_pos != Vec2{FIXTURE_TARGET_X, FIXTURE_TARGET_Y}) {
            action.request.mode = NavMode::Navigate;
            action.request.has_target = true;
            action.request.target = {FIXTURE_TARGET_X, FIXTURE_TARGET_Y};
            return action;
        }

       

        // 4. Patrol ring around spawn position (Radius 1 adjacent steps)
        static const std::array<std::pair<int, int>, 8> circle_offsets = {{
            {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}
        }};

        auto [dx, dy] = circle_offsets[waypoint_index];
        Vec2 target_waypoint = wrap_offset(spawn_position, dx, dy, memory.width, memory.height);

        // Advance waypoint if target reached OR stuck trying to reach it for > 5 turns
        if (current_pos == target_waypoint || ++turns_on_current_waypoint > 5) {
            turns_on_current_waypoint = 0;
            waypoint_index = (waypoint_index + 1) % circle_offsets.size();
            auto [next_dx, next_dy] = circle_offsets[waypoint_index];
            target_waypoint = wrap_offset(spawn_position, next_dx, next_dy, memory.width, memory.height);
        }

        action.request.mode = NavMode::Navigate;
        action.request.has_target = true;
        action.request.target = target_waypoint;

        return action;
    }
};

Role& queen_role() {
    static QueenRole role;
    return role;
}

} // namespace bot::roles