#pragma once

#include <cstdint>
#include <vector>

std::uint8_t authenticate(
    std::uint64_t round,
    std::uint8_t sonar_id,
    std::uint64_t k0,
    std::uint64_t k1
);

std::uint64_t enemy_queen_sonar(
    std::uint8_t x,
    std::uint8_t y,
    std::uint8_t propagate,
    std::uint64_t round
);

std::uint64_t idle_sonar(
    std::uint8_t hx,
    std::uint8_t hy,
    const std::vector<std::vector<bool>>& pearl_grid,
    std::uint64_t round
);

std::uint64_t portal_sonar(
    std::uint8_t sx,
    std::uint8_t sy,
    std::uint8_t sdir,
    std::uint8_t x1,
    std::uint8_t y1,
    std::uint8_t x2,
    std::uint8_t y2,
    std::uint8_t dir2,
    std::uint8_t x3,
    std::uint8_t y3,
    std::uint8_t dir3,
    std::uint64_t round
);

std::uint64_t suicide_sonar(std::uint64_t round);

std::vector<std::uint64_t> decode_sonar(
    std::uint64_t msg,
    std::uint64_t round
);

std::uint64_t queen_killed_sonar(
    std::uint64_t round
);

// Creates a sonar message containing a food location and an 8-bit food value.
std::uint64_t food_signal(
    std::uint8_t x,
    std::uint8_t y,
    std::uint8_t value,
    std::uint64_t round
);