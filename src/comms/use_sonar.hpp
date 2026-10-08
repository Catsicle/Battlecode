#pragma once
#include <cstdint>
#include "../helper.hpp"
#include "comms.hpp"
#include "comm_function.hpp"

namespace bot::comms {

std::uint8_t compute_propagate(const Inbox& inbox);
std::uint64_t maybe_emit_enemy_queen_sonar(
    const unswbc::Controller& controller,
    const Inbox& inbox,
    std::uint64_t round_num);

std::uint64_t maybe_emit_queen_beacon_sonar(
    const unswbc::Controller& controller,
    const Inbox& inbox,
    std::uint64_t round_num);

} // namespace bot::comms