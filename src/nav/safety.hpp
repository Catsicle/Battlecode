#pragma once

#include "sim.hpp"

namespace bot::nav {

bool trap_ok(const Memory&, const Sim&);
bool survives(const Memory&, Sim&, int depth);
bool validate_prefix(const Memory&, const Path&, int count, Sim&, Fail&);

} // namespace bot::nav
