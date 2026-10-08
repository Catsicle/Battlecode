#pragma once

#include "grid.hpp"

namespace bot::nav {

enum class StepKind { Normal, PortalOK, PortalUnresolved, Kelp };

struct Step {
    int dest = -1;
    std::uint8_t new_dir = 0;
    StepKind kind = StepKind::Normal;
};

Step resolve_step(const Memory& memory, int tile, int direction);

} // namespace bot::nav
