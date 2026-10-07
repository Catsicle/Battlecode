#include "role.hpp"
#include "../config.hpp"
#include "../core/body_tracker.hpp"
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
               const nav::Grid& grid, const comms::Inbox& inbox,const bot::BodyTracker& tracker) override {
        (void)grid;
        Action action;
        Vec2 current_pos = controller.get_position();

        // Emit enemy queen sonar if enemy queen body/head visible
        Vec2 queen_spot;
        bool queen_spotted = false;
        for (const auto& tile : controller.get_vision().get_tiles()) {
            const auto* dragon = tile.get_dragon();
            if (dragon != nullptr && dragon->get_team() != controller.get_team()) {
                queen_spotted = true;
                queen_spot = tile.get_position();
                break;
            }
        }
        if (queen_spotted) {
            std::uint64_t sonar_val = enemy_queen_sonar(
                static_cast<std::uint8_t>(queen_spot.x & 0x3F),
                static_cast<std::uint8_t>(queen_spot.y & 0x3F),
                3, action.round_num);
            action.sonar.North = sonar_val;
            action.sonar.East = sonar_val;
            action.sonar.South = sonar_val;
            action.sonar.West = sonar_val;
        }

        // 1. Save initial spawn location on first turn
        if (!initialized) {
            spawn_position = current_pos;
            initialized = true;
        }

        // 2. HIGHEST PRIORITY: NavMode::Navigate Signal from Sonar / Inbox
        // if (inbox.navigation_target.available) {
        //     action.request.mode = NavMode::Navigate;
        //     action.request.has_target = true;
        //     action.request.target = inbox.navigation_target.target;
        //     action.do_split = false; // Bypasses splitting when responding to sonar
        //     return action;
        // }

        // 3. Fixture override (if navigation testing fixture is enabled)
        if (NAVIGATION_FIXTURE && current_pos != Vec2{FIXTURE_TARGET_X, FIXTURE_TARGET_Y}) {
            action.request.mode = NavMode::Navigate;
            action.request.has_target = true;
            action.request.target = {FIXTURE_TARGET_X, FIXTURE_TARGET_Y};
            return action;
        }

        // 4. SPLITTING LOGIC: Split whenever length >= 7 (detaching rest so Queen keeps size 5)
        int current_length = controller.get_length();
        if (current_length >= 7) {
            int split_amount = current_length - 5;
            if (controller.can_split(split_amount)) {
                action.do_split = true;
                action.split_size = split_amount;
                return action;
            }
        }

        // 5. NAVIGATION STRATEGY
        // PHASE 1: Rounds <= 50 -> Follow Explorer logic
        if (tracker.stats.splits <= 2) {
            action.request.mode = NavMode::Explore;
            return action;
        }

        // PHASE 2: Rounds > 50 -> Check circle patrol feasibility
        static const std::array<std::pair<int, int>, 8> circle_offsets = {{
            {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}
        }};

        auto [dx, dy] = circle_offsets[waypoint_index];
        Vec2 target_waypoint = wrap_offset(spawn_position, dx, dy, memory.width, memory.height);

        // Advance to next waypoint if reached or if stuck trying to reach it for > 5 turns
        if (current_pos == target_waypoint || ++turns_on_current_waypoint > 5) {
            turns_on_current_waypoint = 0;
            waypoint_index = (waypoint_index + 1) % circle_offsets.size();
            auto [next_dx, next_dy] = circle_offsets[waypoint_index];
            target_waypoint = wrap_offset(spawn_position, next_dx, next_dy, memory.width, memory.height);
        }

        // Check if target waypoint is reachable/passable
        int target_cell = memory.index(target_waypoint);
        bool circle_feasible = true;
        if (target_cell >= 0 && memory.known(target_cell)) {
            if (memory.kelp[target_cell] == 0x0F) { // Fully blocked by kelp
                circle_feasible = false;
            }
        }

        if (circle_feasible) {
            action.request.mode = NavMode::Navigate;
            action.request.has_target = true;
            action.request.target = target_waypoint;
        } else {
            // Fall back to explore mode if circle is physically blocked
            action.request.mode = NavMode::Explore;
        }

        return action;
    }
};

Role& queen_role() {
    static QueenRole role;
    return role;
}

} // namespace bot::roles