#include "role.hpp"
#include "../comms/use_sonar.hpp"
#include "../config.hpp"
#include "killer/kill.hpp"

namespace bot::roles {

class KillerRole final : public Role {
  private:
    enum class State { Feeding, SearchQueen };
    State state = State::Feeding;

  public:
    Action act(const unswbc::Controller& controller, const Memory& memory,
               const nav::Grid& grid, const comms::Inbox& inbox, const bot::BodyTracker& tracker) override {
        (void)memory;
        (void)grid;
        (void)tracker;
        Action action;

        if (inbox.suicide_exists) {
            action.request.mode = NavMode::Suicide;
            return action;
        }

        Vec2 current_pos = controller.get_position();

        if (inbox.suicide_exists) {
            std::cout << "DEBUG: Suicide message received - committing suicide" << std::endl;
            Action suicide_action;
            suicide_action.request.mode = NavMode::Suicide;
            return suicide_action;
        }

        // Placeholder: detect QueenLocation message type in inbox
        Vec2 queen_location;
        std::uint64_t round_received = 0;
        if (inbox.enemy_queen_exists) {
            for (auto msg : inbox.enemy_queen_messages) {
                if (msg.round > round_received) {
                    queen_location = {msg.x, msg.y};
                    round_received = msg.round;
                }
            }
            state = State::SearchQueen;
        }

        // Emit enemy queen sonar if queen body/head visible
        std::uint64_t sonar_val = comms::maybe_emit_enemy_queen_sonar(controller, inbox, action.round_num);
        if (sonar_val != 0) {
            action.sonar.North = sonar_val;
            action.sonar.East = sonar_val;
            action.sonar.South = sonar_val;
            action.sonar.West = sonar_val;
        }

        // Feeding behavior (default)
        if (state == State::Feeding) {
            bool enemy_head_visible = false;
            unswbc::Position enemy_head_pos{};
            for (const auto& tile : controller.get_vision().get_tiles()) {
                const auto* dragon = tile.get_dragon();
                if (dragon != nullptr && dragon->is_head() && (dragon->get_id() == 0 || dragon->get_id() == 1) &&
                    dragon->get_team() != controller.get_team()) {
                    enemy_head_visible = true;
                    enemy_head_pos = tile.get_position();
                    break;
                }
            }
            if (enemy_head_visible) {
                bot::KillResult kill_result = bot::kill(
                    controller.get_vision(), enemy_head_pos, controller.get_length());
                switch (kill_result.action) {
                    case bot::KillAction::NONE: {
                        int length = controller.get_length();
                        if (length >= 8) {
                            std::cout << "Trying to split" << std::endl;
                            action.do_split = true;
                            action.split_size = length - 6;
                            return action;
                        }
                        action.request.mode = NavMode::Explore;
                        break;
                    }
                    case bot::KillAction::KILL_QUEEN:
                    case bot::KillAction::KILL_ENEMY_HEAD:
                        action.request.mode = NavMode::Direct;
                        action.request.path.clear();
                        for (char c : kill_result.path) {
                            action.request.path.push_back(Dir(c));
                        }
                        break;
                    case bot::KillAction::PREPARE_QUEEN:
                        action.request.mode = NavMode::Navigate;
                        action.request.has_target = true;
                        action.request.target = enemy_head_pos;
                        break;
                }
            } else {
                int length = controller.get_length();
                if (length >= 8) {
                    std::cout << "Trying to split" << std::endl;
                    action.do_split = true;
                    action.split_size = length - 6;
                    return action;
                }
                
                action.request.mode = NavMode::Explore;
            }
            return action;
        }

        
        // SearchQueen behavior
        if (state == State::SearchQueen) {
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
                switch (kill_result.action) {
                    case bot::KillAction::NONE:
                        action.request.mode = NavMode::Navigate;
                        action.request.has_target = true;
                        if (queen_location.x != 0 || queen_location.y != 0) {
                            action.request.target = queen_location;
                        } else {
                            action.request.target = current_pos;
                        }
                        break;
                    case bot::KillAction::KILL_QUEEN:
                    case bot::KillAction::KILL_ENEMY_HEAD:
                        action.request.mode = NavMode::Direct;
                        action.request.path.clear();
                        for (char c : kill_result.path) {
                            action.request.path.push_back(Dir(c));
                        }
                        break;
                    case bot::KillAction::PREPARE_QUEEN:
                        action.request.mode = NavMode::Navigate;
                        action.request.has_target = true;
                        action.request.target = queen_head_pos;
                        break;
                }
            } else {
                action.request.mode = NavMode::Navigate;
                action.request.has_target = true;
                if (queen_location.x != 0 || queen_location.y != 0) {
                    action.request.target = queen_location;
                } else {
                    // Placeholder: fallback target from sonar/inbox location
                    action.request.target = current_pos;
                }
            }
            return action;
        }

        // Fallback
        action.request.mode = NavMode::Explore;
        return action;
    }
};

Role& killer_role() {
    static KillerRole role;
    return role;
}

} // namespace bot::roles
