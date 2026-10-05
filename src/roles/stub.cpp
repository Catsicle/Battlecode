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
               const nav::Grid& grid, const comms::Inbox& inbox) override {
        (void)memory;
        Action action;
        if (inbox.navigation_target.available) {
            action.path = nav::search(grid, controller.get_position(), inbox.navigation_target.target,
                                      true, controller.get_length());
        } else if (NAVIGATION_FIXTURE &&
                   controller.get_position() != Vec2{FIXTURE_TARGET_X, FIXTURE_TARGET_Y}) {
            action.path = nav::approach_target(
                grid,
                controller.get_position(),
                {FIXTURE_TARGET_X, FIXTURE_TARGET_Y},
                controller.get_length());
        } else {
            action.path = nav::wander(grid, controller.get_position(), controller.get_length());
        }
        return action;
    }
};

Role& stub_role() {
    static StubRole role;
    return role;
}

} // namespace bot::roles
