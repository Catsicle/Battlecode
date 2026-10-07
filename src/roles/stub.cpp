#include "role.hpp"
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
        Action action;
        if (inbox.navigation_target.available) {
            action.request.mode = NavMode::Navigate;
            action.request.has_target = true;
            action.request.target = inbox.navigation_target.target;
        } else if (NAVIGATION_FIXTURE &&
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
