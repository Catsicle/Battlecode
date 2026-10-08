#pragma once
#include "grid.hpp"
#include <cstdint>

namespace bot::nav::scout {

// Call once per turn (scout mode only) before planning. Resolves a crossing made last turn.
void begin_turn(const Memory& memory, Vec2 head);

// Plan the next path. A path that ends by crossing a portal has the portal step as its last step.
Path plan(const Grid& grid, Vec2 start, std::uint8_t forbid_mask);

// True if (cell, dir) is the portal step the last plan() ended with.
bool is_planned_crossing(int cell, int dir);

// Record that we are about to step through the portal edge (cell, dir).
void note_crossing(const Memory& memory, int cell, int dir);

} // namespace bot::nav::scout