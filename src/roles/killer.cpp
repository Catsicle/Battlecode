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
        action.request.mode = NavMode::Explore;
        return action;
    }
};

Role& killer_role() {
    static KillerRole role;
    return role;
}

} // namespace bot::roles