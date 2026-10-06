#pragma once

#include "../../core/memory.hpp"
#include "../../core/memory/queries.hpp"
#include "fixed_bitset.hpp"
#include "constants.hpp"

namespace bot::bodyguard::core {

using Cell = int;
using Position = bot::Vec2;
using Direction = bot::Dir;

struct Grid {
    const bot::Memory& memory;
    int W = 0;
    int H = 0;

    Grid(const bot::Memory& mem, int w, int h) : memory(mem), W(w), H(h) {}

    bool usable(Cell c) const {
        auto [v, _] = bot::get_cell(memory, c % W, c / W);
        return v.has_value() && v.value() >= 0;
    }
    bool plain(Cell c, Direction d) const {
        auto [edge_v, edge_id] = bot::get_edge(memory, c % W, c / W, static_cast<int>(d.value) - 'E');
        return edge_v.has_value() && edge_v.value() == 0;
    }
};

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

struct PocketRegion {
    FixedBitset<MAX_CELLS> cells;
    int size = 0;
    bool touches_exterior = false;
};

} // namespace bot::bodyguard::core