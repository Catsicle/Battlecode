#include "role.hpp"

namespace bot::roles {

class FeederRole final : public Role {
  public:
    Action act(const unswbc::Controller& controller, const Memory& memory,
               const nav::Grid& grid, const comms::Inbox& inbox,const bot::BodyTracker& tracker) override {
        (void)controller;
        (void)memory;
        (void)grid;
        (void)inbox;
        (void)tracker;

        Action action;
        // Explores map for resources using wander scoring and safety fallbacks
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

Role& feeder_role() {
    static FeederRole role;
    return role;
}

} // namespace bot::roles