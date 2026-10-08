#pragma once

#include "../roles/role.hpp"
#include "sim.hpp"

namespace bot::nav {

Path plan(const roles::Action::PlanRequest&, const Grid&, const Sim&, Vec2 start, std::uint8_t forbid_mask);

} // namespace bot::nav
