#pragma once

#include "types.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace bot {

struct Occupancy {
    bool present = false;
    bool head = false;
    char team = 0;
    int id = -1;
    int round = -1;
};

struct Memory {
    int width = 0;
    int height = 0;
    int cells = 0;
    std::vector<std::uint8_t> kelp;
    std::vector<std::uint8_t> known_edges;
    std::vector<std::array<std::int16_t, 4>> portal_id;
    std::vector<std::array<bool, 4>> portal_exit_known;
    std::vector<bool> seen;
    std::vector<int> last_seen_round;
    std::vector<int> pearl_spawn_round;
    std::vector<bool> never_spawns;
    std::vector<int> visits_nopearl;
    std::vector<int> last_visit_round;
    std::vector<bool> eaten_here;
    std::vector<Occupancy> occupancy;
    std::vector<bool> pearl;
    std::vector<Vec2> own_body;
    Vec2 previous_head{};
    bool has_previous_head = false;
    bool previous_move_ate = false;

    void initialize(int map_width, int map_height);
    void update(const unswbc::Controller& controller, int round);
    bool known(int cell) const;
    bool edge_known(int cell, int direction) const;
    bool is_frontier(int cell) const;
    int index(Vec2 position) const { return tile_index(position, width); }
    Vec2 position(int cell) const { return {cell % width, cell / width}; }
};

} // namespace bot
