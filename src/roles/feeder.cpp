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
        action.request.mode = NavMode::Explore;
        return action;
    }
};

Role& feeder_role() {
    static FeederRole role;
    return role;
}

} // namespace bot::roles