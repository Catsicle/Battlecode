#include "config.hpp"
#include "core/memory.hpp"
#include "comms/comms.hpp"
#include "roles/role.hpp"
#include "core/body_tracker.hpp"
#include "nav/executor.hpp"

#include <algorithm>
#include <vector>

namespace bot::roles { Role& select_role(); }
int main() {
    auto [controller, game] = unswbc::init();
    bot::Memory memory;
    auto [width, height] = game.get_map_size();
    memory.initialize(width, height);
    bot::roles::Role& role = bot::roles::select_role();
    bot::BodyTracker tracker;

    while (unswbc::update(controller, game)) {
        memory.update(controller, game.get_round_num());
        tracker.reconcile(controller, memory);
        bot::comms::Inbox inbox = bot::comms::read_inbox(controller);
        bot::nav::Grid grid{memory, game.get_round_num()};
        bot::roles::Action action = role.act(controller, memory, grid, inbox);
        bot::nav::execute_turn(controller, memory, tracker, grid, action.request);
        unswbc::end_turn();
    }
}
