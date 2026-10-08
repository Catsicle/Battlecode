#include "role.hpp"

namespace bot::roles {

class KillerRole final : public Role {
  public:
    Action act(const unswbc::Controller& controller, const Memory& memory,
               const nav::Grid& grid, const comms::Inbox& inbox,const bot::BodyTracker& tracker) override {
        (void)controller;
        (void)memory;
        (void)grid;
        (void)inbox;
        (void)tracker;

        Action action;
        // Placeholder: Default exploration mode until strategy is defined
        int current_length = controller.get_length();
         if (current_length >= 4) {
            int split_amount = current_length - 2;
            if (controller.can_split(split_amount)) {
                action.do_split = true;
                action.split_size = split_amount;
                return action;
            }
        }
        action.request.mode = NavMode::Scout;
        return action;
    }
};

Role& killer_role() {
    static KillerRole role;
    return role;
}

} // namespace bot::roles