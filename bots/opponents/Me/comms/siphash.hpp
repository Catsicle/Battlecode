#pragma once

#include <cstdint>

std::uint64_t siphash24(
    std::uint64_t m,
    std::uint64_t k0,
    std::uint64_t k1
);
