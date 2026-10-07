#include "role.hpp"
#include "../config.hpp"
#include "killer/kill.hpp"

namespace bot::roles {

class KillerRole final : public Role {
  private:
    enum class State { Feeding, SearchQueen };
    State state = State::Feeding;

  public:
    Action act(const unswbc::Controller& controller, const Memory& memory,
               const nav::Grid& grid, const comms::Inbox& inbox) override {
        (void)memory;
        (void)grid;
        Action action;
        Vec2 current_pos = controller.get_position();

        // Placeholder: detect QueenLocation message type in inbox
        bool received_queen_location = false;
        for (const auto& msg : inbox.messages) {
            // Placeholder for MessageType::QueenLocation (not yet defined in msg.hpp)
            if (msg.type == static_cast<bot::comms::MessageType>(4)) {
                received_queen_location = true;
            }
        }
        if (received_queen_location) {
            state = State::SearchQueen;
        }

        // Feeding behavior (default)
        if (state == State::Feeding) {
            bool enemy_head_visible = false;
            unswbc::Position enemy_head_pos{};
            for (const auto& tile : controller.get_vision().get_tiles()) {
                const auto* dragon = tile.get_dragon();
                if (dragon != nullptr && dragon->is_head() &&
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
                        if (length >= 7) {
                            action.do_split = true;
                            action.split_size = length - 5;
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
                if (length >= 7) {
                    action.do_split = true;
                    action.split_size = length - 5;
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
                switch (kill_result.action) {
                    case bot::KillAction::NONE:
                        action.request.mode = NavMode::Navigate;
                        action.request.has_target = true;
                        if (inbox.navigation_target.available) {
                            action.request.target = inbox.navigation_target.target;
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
                if (inbox.navigation_target.available) {
                    action.request.target = inbox.navigation_target.target;
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
