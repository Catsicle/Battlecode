#include "config.hpp"
#include "core/memory.hpp"
#include "comms/comms.hpp"
#include "roles/role.hpp"
#include "roles/selector.hpp"
#include "core/body_tracker.hpp"
#include "nav/executor.hpp"

#include <algorithm>
#include <iostream>
#include <set>
#include <vector>

namespace bot::roles {
extern Role& queen_role();
extern Role& explorer_role();
extern Role& killer_role();
extern Role& feeder_role();
} // namespace bot::roles

int main() {
    auto [controller, game] = unswbc::init();
    bot::Memory memory;
    auto [width, height] = game.get_map_size();
    memory.initialize(width, height);
    
    bot::BodyTracker tracker;
    // std::set<int> seen_bots;

    while (unswbc::update(controller, game)) {
        memory.update(controller, game.get_round_num());
        tracker.reconcile(controller, memory);
        bot::comms::Inbox inbox = bot::comms::read_inbox(controller,game.get_round_num());
        bot::nav::Grid grid{memory, game.get_round_num()};
        
        bot::roles::Role& active_role = bot::roles::select_role(controller, game.get_round_num());
        
        // int bot_id = controller.get_id();
        // if (seen_bots.find(bot_id) == seen_bots.end()) {
        //     if (&active_role == &bot::roles::queen_role()) {
        //         std::cout << "queen:" << bot_id << "\n";
        //     } else if (&active_role == &bot::roles::explorer_role()) {
        //         std::cout << "explorer:" << bot_id << "\n";
        //     } else if (&active_role == &bot::roles::killer_role()) {
        //         std::cout << "killer:" << bot_id << "\n";
        //     } else if (&active_role == &bot::roles::feeder_role()) {
        //         std::cout << "feeder:" << bot_id << "\n";
        //     }
        //     seen_bots.insert(bot_id);
        // }

        bot::roles::Action action = active_role.act(controller, memory, grid, inbox,tracker);
        action.round_num = static_cast<std::uint64_t>(game.get_round_num());
        
        
        if (action.do_split && controller.can_split(action.split_size)) {
            controller.do_split(action.split_size);
            tracker.reset();
        } else {
            bot::nav::execute_turn(controller, memory, tracker, grid, action, &active_role);
        }

        // Emit sonar messages
        if (action.sonar.North != 0) {
            std::cout << "DEBUG: Emitting sonar North=" << action.sonar.North << std::endl;
            controller.send_sonar(unswbc::Direction::NORTH, action.sonar.North);
        }
        if (action.sonar.East != 0) {
            std::cout << "DEBUG: Emitting sonar East=" << action.sonar.East << std::endl;
            controller.send_sonar(unswbc::Direction::EAST, action.sonar.East);
        }
        if (action.sonar.South != 0) {
            std::cout << "DEBUG: Emitting sonar South=" << action.sonar.South << std::endl;
            controller.send_sonar(unswbc::Direction::SOUTH, action.sonar.South);
        }
        if (action.sonar.West != 0) {
            std::cout << "DEBUG: Emitting sonar West=" << action.sonar.West << std::endl;
            controller.send_sonar(unswbc::Direction::WEST, action.sonar.West);
        }
        if (action.sonar.East != 0)
            controller.send_sonar(unswbc::Direction::EAST, action.sonar.East);
        if (action.sonar.South != 0)
            controller.send_sonar(unswbc::Direction::SOUTH, action.sonar.South);
        if (action.sonar.West != 0)
            controller.send_sonar(unswbc::Direction::WEST, action.sonar.West);

        unswbc::end_turn();
    }
}