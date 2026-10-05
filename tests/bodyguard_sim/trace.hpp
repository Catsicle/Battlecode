#pragma once

#include "sim.hpp"

#include <cstdint>

namespace bodyguard_sim {

struct TraceScript {
    std::uint32_t seed = 0;
    int rounds = 32;

    Direction direction(int round, int dragon_id) const {
        return static_cast<Direction>((seed + static_cast<std::uint32_t>(round * 3 + dragon_id * 5)) % 4U);
    }

    TurnAction action(int round, int dragon_id) const {
        Action movement;
        movement.push(direction(round, dragon_id));
        return TurnAction::move(movement);
    }
};

} // namespace bodyguard_sim
