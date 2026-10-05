#pragma once

#include "types.hpp"

namespace bot::bodyguard::core {

inline Position neighbor(const Memory& memory, Cell cell, Direction direction) {
    return add(memory.position(cell), direction, memory.width, memory.height);
}

inline Cell neighbor_cell(const Memory& memory, Cell cell, Direction direction) {
    return memory.index(neighbor(memory, cell, direction));
}

} // namespace bot::bodyguard::core