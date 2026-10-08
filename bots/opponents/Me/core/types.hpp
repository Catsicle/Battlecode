#pragma once

#include "../helper.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace bot {

using Vec2 = unswbc::Position;
using Dir = unswbc::Direction;

inline std::array<Dir, 4> directions() {
    return {Dir::NORTH, Dir::EAST, Dir::SOUTH, Dir::WEST};
}

inline int dir_index(Dir direction) {
    switch (direction.value) {
    case Dir::NORTH: return 0;
    case Dir::EAST: return 1;
    case Dir::SOUTH: return 2;
    default: return 3;
    }
}

inline Vec2 add(Vec2 position, Dir direction, int width, int height) {
    auto [dx, dy] = direction.get_offset();
    auto wrap = [](int value, int size) { return (value % size + size) % size; };
    return {wrap(position.x + dx, width), wrap(position.y + dy, height)};
}

inline int tile_index(Vec2 position, int width) {
    return position.y * width + position.x;
}

struct Path {
    std::vector<Dir> steps;
    int length = 0;
    int cost = 0;
    bool found = false;
};

enum class NavMode {
    Explore,
    Navigate,
    Direct,
    Suicide,
};

} // namespace bot
