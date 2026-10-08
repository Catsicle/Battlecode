#include "selector.hpp"
#include "../config.hpp"

namespace bot::roles {

// Forward-declare factory functions for each role implementation
extern Role& queen_role();
extern Role& explorer_role();
extern Role& killer_role();
extern Role& feeder_role();
extern Role& bodyguard_role();

Role& select_role(const unswbc::Controller& controller, int round) {
    int id = controller.get_id();

    // Rule 1: IDs 0 and 1 are Queens
    if (id == 0 || id == 1) {
        return queen_role();
    }

    // Rule 2: IDs where id % 4 == 0 (and id > 1) are Explorers
    if (id % 4 == 0) {
        return explorer_role();
    }

    int mod = id % 4;

    // Rule 3: Phase 1 (Rounds < 350)
    if (round < 350) {
        if (mod == 1 || mod == 2) {
            return bodyguard_role();
        } else { // mod == 3
            return feeder_role();
        }
    }

    // Rule 4: Phase 2 (Rounds >= 350)
    if (mod == 1 || mod == 2) {
        return bodyguard_role();
    } else { // mod == 3
        return killer_role();
    }
}

} // namespace bot::roles