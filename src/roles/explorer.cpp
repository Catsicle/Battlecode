#include "role.hpp"
#include "../comms/use_sonar.hpp"
#include "killer/kill.hpp"

namespace bot::roles {

class ExplorerRole final : public Role {
  public:
    Action act(const unswbc::Controller& controller, const Memory& memory,
               const nav::Grid& grid, const comms::Inbox& inbox,const bot::BodyTracker& tracker) override {
        (void)controller;
        (void)memory;
        (void)grid;
        (void)inbox;
        (void)tracker;

        if (inbox.suicide_exists) {
            Action suicide_action;
            suicide_action.request.mode = NavMode::Suicide;
            return suicide_action;
        }

        Action action;

        // Emit enemy queen sonar if queen body/head visible
                std::uint64_t sonar_val = comms::maybe_emit_enemy_queen_sonar(controller, inbox, action.round_num);
        if (sonar_val != 0) {
            action.sonar.North = sonar_val;
            action.sonar.East = sonar_val;
            action.sonar.South = sonar_val;
            action.sonar.West = sonar_val;
        }


        bool queen_head_visible = false;
        unswbc::Position queen_head_pos{};
        for (const auto& tile : controller.get_vision().get_tiles()) {
            const auto* dragon = tile.get_dragon();
            if (dragon != nullptr && dragon->is_head() && (dragon->get_id() == 0 || dragon->get_id() == 1) &&
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

        // Placeholder: Default exploration mode until strategy is defined
        action.request.mode = NavMode::Explore;
        return action;
    }
};

Role& explorer_role() {
    static ExplorerRole role;
    return role;
}

} // namespace bot::roles