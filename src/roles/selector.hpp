#pragma once

#include "role.hpp"

namespace bot::roles {

// Returns the active role for a bot given its controller state and the game round
Role& select_role(const unswbc::Controller& controller, int round);

} // namespace bot::roles