#pragma once
#include "bodyguard/core/torus.hpp"
#include "bodyguard/core/types.hpp"
#include <array>
#include <cstdint>

constexpr int MAX_CYCLE_LEN = 64;

namespace bodyguard {
using bot::bodyguard::core::Cell;

// Validator (M3): given a candidate cycle C (array of cells, even length),
// check simplicity, kelp-free edges, pocket sealing, and dash feasibility.
// Fixed-size, no heap per call.
bool is_simple_cycle(const std::array<Cell, MAX_CYCLE_LEN>& cycle, int len);

// After removing C tiles and kelp edges, flood-fill from pocket P.
// Return bounded==true if component does not reach exterior.
bool seals_pocket(const std::array<Cell, MAX_CYCLE_LEN>& cycle, int len, Cell pocket);

// Pick door X on cycle, outside tile D adjacent to X, and a dash path
// of s steps from D to X that avoids collision with C.
bool find_dash(const std::array<Cell, MAX_CYCLE_LEN>& cycle, int len, int s, Cell& door, Cell& outside);

} // namespace bodyguard
