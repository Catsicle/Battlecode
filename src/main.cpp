#include "config.hpp"
#include "core/memory.hpp"
#include "comms/comms.hpp"
#include "roles/role.hpp"

#include <algorithm>
#include <vector>

namespace bot::roles { Role& select_role(); }
namespace bot::nav { extern Dir safest_move(const Grid&, Vec2, bool&); }

int main() {
    auto [controller, game] = unswbc::init();
    bot::Memory memory;
    auto [width, height] = game.get_map_size();
    memory.initialize(width, height);
    bot::roles::Role& role = bot::roles::select_role();

    while (unswbc::update(controller, game)) {
        memory.update(controller, game.get_round_num());
        bot::comms::Inbox inbox = bot::comms::read_inbox(controller);
        bot::nav::Grid grid{memory, game.get_round_num()};
        bot::roles::Action action = role.act(controller, memory, grid, inbox);
        int free_steps = (controller.get_length() + 3) / 4;
        // Prefix validation is currently one-step exact; multi-step emission
        // will be enabled once body replay is wired into the safety module.
        int emit_count = std::min({free_steps, action.path.length, 1});
        if (action.path.found && emit_count > 0) {
            std::vector<unswbc::Direction> moves;
            for (int i = 0; i < emit_count; ++i)
                moves.push_back(action.path.steps[i]);
            controller.make_moves(moves);
        } else {
            bool found = false;
            bot::Dir safe = bot::nav::safest_move(grid, controller.get_position(), found);
            if (found)
                controller.make_move(safe);
            else if (controller.can_split(controller.get_length() - 2))
                controller.do_split(controller.get_length() - 2);
            else
                controller.make_move(controller.get_dir());
        }
        unswbc::end_turn();
    }
}
