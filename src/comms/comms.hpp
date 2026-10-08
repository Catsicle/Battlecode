#pragma once

#include "comm_function.hpp"
#include "../helper.hpp"
#include <cstdint>
#include <vector>

namespace bot::comms {

// Message type structs (separate per sonar ID)
struct enemy_queen_sonar {
    std::uint8_t x = 0;
    std::uint8_t y = 0;
    std::uint8_t propagate = 3;
    std::uint64_t round = 0;
};

struct idle_sonar {
    std::uint8_t hx = 0;
    std::uint8_t hy = 0;
    std::vector<bool> pearl_grid; // 36 bits for 6x6 grid
    std::uint64_t round = 0;
};

struct portal_sonar {
    std::uint8_t sx = 0;
    std::uint8_t sy = 0;
    std::uint8_t sdir = 0;
    std::uint8_t x1 = 0;
    std::uint8_t y1 = 0;
    std::uint8_t x2 = 0;
    std::uint8_t y2 = 0;
    std::uint8_t dir2 = 0;
    std::uint8_t x3 = 0;
    std::uint8_t y3 = 0;
    std::uint8_t dir3 = 0;
    std::uint64_t round = 0;
};

struct suicide_sonar {
    std::uint64_t secret = 0;
    std::uint64_t round = 0;
};

struct queen_killed_sonar {
    std::uint64_t round = 0;
};

struct food_signal_sonar {
    std::uint8_t x = 0;
    std::uint8_t y = 0;
    std::uint8_t value = 0;
    std::uint64_t round = 0;
};

// Placeholder beacon sonar — matches existing sonar message style
struct queen_beacon_sonar {
    std::uint8_t x = 0;
    std::uint8_t y = 0;
    std::uint64_t round = 0;
};

struct Inbox {
    std::vector<enemy_queen_sonar> enemy_queen_messages;
    bool enemy_queen_exists = false;

    std::vector<idle_sonar> idle_messages;
    bool idle_exists = false;

    std::vector<portal_sonar> portal_messages;
    bool portal_exists = false;

    std::vector<suicide_sonar> suicide_messages;
    bool suicide_exists = false;

    std::vector<queen_killed_sonar> queen_killed_messages;
    bool queen_killed_exists = false;

    std::vector<food_signal_sonar> food_signal_messages;
    bool food_signal_exists = false;

    std::vector<queen_beacon_sonar> queen_beacon_messages;
    bool queen_beacon_exists = false;
};

Inbox read_inbox(
    const unswbc::Controller& controller,
    std::uint64_t round
);

} // namespace bot::comms
