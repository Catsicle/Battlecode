#pragma once

#include <array>
#include <cassert>
#include <cstdlib>
#include "../core/types.hpp"

namespace bot::roles {

enum class StepDir {
    Up,
    Right,
    Down,
    Left
};

struct RingBox {
    int left;
    int top;
    int right;
    int bottom;
};

struct RingPath {
    static constexpr int MAX_CELLS = 1024;

    std::array<Vec2, MAX_CELLS> cells{};
    std::array<StepDir, MAX_CELLS> moves{};
    int count = 0;

    bool push(const Vec2& cell) {
        if (count >= MAX_CELLS) {
            return false;
        }
        cells[count++] = cell;
        return true;
    }
};

inline StepDir direction_between(const Vec2& a, const Vec2& b) {
    const int dx = b.x - a.x;
    const int dy = b.y - a.y;
    assert(std::abs(dx) + std::abs(dy) == 1);
    if (dx == 1) {
        return StepDir::Right;
    }
    if (dx == -1) {
        return StepDir::Left;
    }
    if (dy == 1) {
        return StepDir::Down;
    }
    return StepDir::Up;
}

inline RingPath make_rectangular_ring(const RingBox& box) {
    RingPath path{};
    if (box.left >= box.right || box.top >= box.bottom) {
        return path;
    }
    for (int x = box.left; x <= box.right; ++x) {
        if (!path.push(Vec2{x, box.top})) {
            return RingPath{};
        }
    }
    for (int y = box.top + 1; y <= box.bottom; ++y) {
        if (!path.push(Vec2{box.right, y})) {
            return RingPath{};
        }
    }
    for (int x = box.right - 1; x >= box.left; --x) {
        if (!path.push(Vec2{x, box.bottom})) {
            return RingPath{};
        }
    }
    for (int y = box.bottom - 1; y > box.top; --y) {
        if (!path.push(Vec2{box.left, y})) {
            return RingPath{};
        }
    }
    for (int i = 0; i < path.count; ++i) {
        const int next = (i + 1) % path.count;
        path.moves[i] = direction_between(path.cells[i], path.cells[next]);
    }
    return path;
}

inline Dir step_dir_to_dir(StepDir s) {
    switch (s) {
        case StepDir::Up: return Dir::NORTH;
        case StepDir::Right: return Dir::EAST;
        case StepDir::Down: return Dir::SOUTH;
        case StepDir::Left: return Dir::WEST;
    }
    return Dir::NORTH;
}

inline bool same_position(const Vec2& a, const Vec2& b) {
    return a.x == b.x && a.y == b.y;
}

inline RingPath rotate_path_to_start(const RingPath& original, const Vec2& start) {
    RingPath result{};
    int start_index = -1;
    for (int i = 0; i < original.count; ++i) {
        if (same_position(original.cells[i], start)) {
            start_index = i;
            break;
        }
    }
    if (start_index == -1) {
        return result;
    }
    for (int i = 0; i < original.count; ++i) {
        const int src = (start_index + i) % original.count;
        result.cells[i] = original.cells[src];
        ++result.count;
    }
    for (int i = 0; i < result.count; ++i) {
        const int next = (i + 1) % result.count;
        result.moves[i] = direction_between(result.cells[i], result.cells[next]);
    }
    return result;
}

} // namespace bot::roles
