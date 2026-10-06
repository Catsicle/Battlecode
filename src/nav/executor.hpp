#pragma once

#include "../core/body_tracker.hpp"
#include "planner.hpp"

namespace bot::nav {

void execute_turn(unswbc::Controller&, Memory&, BodyTracker&,
                  const Grid&, const roles::Action::PlanRequest&);

} // namespace bot::nav
