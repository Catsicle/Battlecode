#pragma once
#include <optional>
#include <utility>
#include "../memory.hpp"

namespace bot {

// Query functions (spec-aligned directions: 0=N, 1=E, 2=S, 3=W)
// Reads from Memory adapter state (usable, seen, occupancy, pearl, kelp, portal_id)

std::pair<std::optional<int>, std::optional<int>> get_cell(const Memory& memory, int x, int y);
std::pair<std::optional<int>, std::optional<int>> get_edge(const Memory& memory, int x, int y, int dir);

} // namespace bot
