#include "role.hpp"
#include "killer/kill.hpp"
#include "../config.hpp"

namespace bot::nav {
extern Path search(const Grid&, Vec2, Vec2, bool, int, int = -1);
extern Path wander(const Grid&, Vec2, int);
extern Path approach_target(const Grid&, Vec2, Vec2, int);
extern Dir safest_move(const Grid&, Vec2, bool&);
}

namespace bot::roles {

class StubRole final : public Role {
  public:
    Action act(const unswbc::Controller& controller, const Memory& memory,
               const nav::Grid& grid, const comms::Inbox& inbox,const bot::BodyTracker& tracker) override {
        (void)memory;(void)tracker;(void)grid;

        if (inbox.suicide_exists) {
            Action suicide_action;
            suicide_action.request.mode = NavMode::Suicide;
            return suicide_action;
        }

        Action action;
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

        bool queen_head_visible = false;
        unswbc::Position queen_head_pos{};
        for (const auto& tile : controller.get_vision().get_tiles()) {
            const auto* dragon = tile.get_dragon();
            if (dragon != nullptr && dragon->is_head() &&
                dragon->get_team() != controller.get_team()) {
                queen_head_visible = true;
                queen_head_pos = tile.get_position();
                break;
            }
        }
        if (queen_head_visible) {
            bot::KillResult kill_result = bot::kill(
                controller.get_vision(), queen_head_pos, controller.get_length());
            if (kill_result.action == bot::KillAction::KILL_QUEEN ||
                kill_result.action == bot::KillAction::KILL_ENEMY_HEAD) {
                action.request.mode = NavMode::Direct;
                action.request.path.clear();
                for (char c : kill_result.path) {
                    action.request.path.push_back(Dir(c));
                }
                return action;
            }
        }

        // if (inbox.navigation_target.available) {
        //     action.request.mode = NavMode::Navigate;
        //     action.request.has_target = true;
        //     action.request.target = inbox.navigation_target.target;
        if (NAVIGATION_FIXTURE &&
                   controller.get_position() != Vec2{FIXTURE_TARGET_X, FIXTURE_TARGET_Y}) {
            action.request.mode = NavMode::Navigate;
            action.request.has_target = true;
            action.request.target = {FIXTURE_TARGET_X, FIXTURE_TARGET_Y};
        } else {
            action.request.mode = NavMode::Explore;
        }
        return action;
    }
};

Role& stub_role() {
    static StubRole role;
    return role;
}

} // namespace bot::roles
