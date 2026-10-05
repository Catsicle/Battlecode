#pragma once
#include "bodyguard/core/torus.hpp"
#include "bodyguard/core/types.hpp"
#include <array>

namespace bodyguard {
using bot::bodyguard::core::Cell;
constexpr int MAX_CYCLE_LEN = 64;

// M2a Rectangle generator: simple rectangle ring anchored around free pocket (even length).
std::array<Cell, MAX_CYCLE_LEN> generate_rectangle(const std::array<Cell, 1024>& window, int window_size, int pocket);

// Generator (b): kelp-aware shortest simple cycle on local ≤32x32 torus window.
// Crosses odd number of kelp edges; result kept only if simple.
std::array<Cell, MAX_CYCLE_LEN> generate_kelp_odd(const std::array<Cell, 1024>& window, int window_size, int pocket);

// Generator (c): chokepoint plug — min vertex cut between pocket and exterior.
// Shortest simple cycle through cut tiles.
std::array<Cell, MAX_CYCLE_LEN> generate_chokepoint(const std::array<Cell, 1024>& window, int window_size, int pocket);

} // namespace bodyguard
