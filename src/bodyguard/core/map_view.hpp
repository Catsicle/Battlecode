#pragma once

#include "torus.hpp"

namespace bot::bodyguard::core {

class MapView {
    const Memory& memory;

  public:
    explicit MapView(const Memory& source) : memory(source) {}

    int width() const { return memory.width; }
    int height() const { return memory.height; }
    int cells() const { return memory.cells; }

    bool valid(Cell cell) const { return cell >= 0 && cell < memory.cells; }
    bool known(Cell cell) const { return memory.known(cell); }
    bool frontier(Cell cell) const { return memory.is_frontier(cell); }
    Position position(Cell cell) const { return memory.position(cell); }
    Cell cell(Position position) const { return memory.index(position); }
    Cell neighbor(Cell source, Direction direction) const { return neighbor_cell(memory, source, direction); }

    bool edge_known(Cell cell, int direction) const { return memory.edge_known(cell, direction); }
    bool kelp(Cell cell, int direction) const {
        return valid(cell) && (memory.kelp[cell] & (std::uint8_t{1} << direction)) != 0;
    }
    std::int16_t portal_id(Cell cell, int direction) const {
        return valid(cell) ? memory.portal_id[cell][direction] : -1;
    }
    bool portal_exit_known(Cell cell, int direction) const {
        return valid(cell) && memory.portal_exit_known[cell][direction];
    }

    bool seen(Cell cell) const { return valid(cell) && memory.seen[cell]; }
    int last_seen_round(Cell cell) const { return valid(cell) ? memory.last_seen_round[cell] : -1; }
    bool has_pearl(Cell cell) const { return valid(cell) && memory.pearl[cell]; }
    int pearl_spawn_round(Cell cell) const {
        return valid(cell) ? memory.pearl_spawn_round[cell] : std::numeric_limits<int>::max();
    }
    bool never_spawns(Cell cell) const { return valid(cell) && memory.never_spawns[cell]; }
    const Occupancy& occupancy(Cell cell) const {
        static const Occupancy empty{};
        return valid(cell) ? memory.occupancy[cell] : empty;
    }
};

} // namespace bot::bodyguard::core