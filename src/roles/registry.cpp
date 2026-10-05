#include "role.hpp"
#include "../config.hpp"

namespace bot::roles {
Role& stub_role();

Role& select_role() {
    switch (FORCE_ROLE) {
    case ForcedRole::Auto:
    case ForcedRole::Stub:
    case ForcedRole::Explorer:
    case ForcedRole::Killer:
    case ForcedRole::Feeder:
    case ForcedRole::Bodyguard:
    case ForcedRole::Queen:
        return stub_role();
    }
    return stub_role();
}

} // namespace bot::roles
