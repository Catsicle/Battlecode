#include "role.hpp"
#include "bodyguard.hpp"
#include "queen.hpp"
#include "../config.hpp"

namespace bot::roles {
Role& stub_role();

Role& select_role() {
    switch (FORCE_ROLE) {
    case ForcedRole::Auto:
    case ForcedRole::Stub:
        return stub_role();
    case ForcedRole::Bodyguard:
        return bodyguard_role();
    case ForcedRole::Explorer:
    case ForcedRole::Killer:
    case ForcedRole::Feeder:
    case ForcedRole::Queen:
        return queen_role();
    }
    return stub_role();
}

} // namespace bot::roles
