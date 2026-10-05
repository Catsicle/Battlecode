#pragma once

#include "../../core/memory.hpp"

namespace bot::bodyguard::core {

using Cell = int;
using Position = bot::Vec2;
using Direction = bot::Dir;

constexpr int MAX_CYCLE_LEN = 64;

struct Cycle {
    std::array<Cell, MAX_CYCLE_LEN> cells{};
    int len = 0;
};

struct CycleCandidate {
    Cycle cycle;
    Cell pocket = -1;
    int door_index = -1;
    int score = 0;
};

} // namespace bot::bodyguard::core