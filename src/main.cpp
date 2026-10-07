#include "config.hpp"
#include "core/memory.hpp"
#include "comms/comms.hpp"
#include "roles/role.hpp"
<<<<<<< Updated upstream
#include "roles/bodyguard/rect_adapter.hpp"
=======
#include "roles/bodyguard.hpp"
#include "roles/queen.hpp"
#include "core/body_tracker.hpp"
#include "nav/executor.hpp"
>>>>>>> Stashed changes

#include <algorithm>
#include <vector>

namespace bot::roles { Role& select_role(); }
namespace bot::nav { extern Dir safest_move(const Grid&, Vec2, bool&); }

int main() {
    auto [controller, game] = unswbc::init();
    bot::Memory memory;
    auto [width, height] = game.get_map_size();
    memory.initialize(width, height);
<<<<<<< Updated upstream
    bot::roles::Role& role = bot::roles::select_role();

    while (unswbc::update(controller, game)) {
        memory.update(controller, game.get_round_num());

        // Print current best box from adapter (static Snapshot to avoid ~100KB wasm stack overflow)
        static bodyguard::rect::Snapshot snap;
        bodyguard::rect_adapter::fill_snapshot(memory, game.get_round_num(), snap);
        int px = controller.get_position().x;
        int py = controller.get_position().y;
        bodyguard::rect::Box out_box[1];
        int found_box = bodyguard::rect_adapter::query_top_boxes(snap, px, py, 24, 1, out_box);
        controller.output_log("Hi");
        if (found_box > 0 && out_box[0].w > 0 && out_box[0].h > 0) {
        
        controller.output_log("BEST_BOX",
            (int)out_box[0].x0, (int)out_box[0].y0,
            (int)out_box[0].w, (int)out_box[0].h,
            (int)out_box[0].c,
            "found", found_box);
        }
=======
    static_cast<bot::roles::BodyguardRole&>(bot::roles::bodyguard_role()).reset();
    bot::BodyTracker tracker;

    while (unswbc::update(controller, game)) {
        memory.update(controller, game.get_round_num());
        tracker.reconcile(controller, memory);
>>>>>>> Stashed changes

        bot::comms::Inbox inbox = bot::comms::read_inbox(controller);
        bot::nav::Grid grid{memory, game.get_round_num()};
        bot::roles::Role& role = (controller.get_id() == 0 || controller.get_id() == 1) ? bot::roles::queen_role() : bot::roles::bodyguard_role();
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
