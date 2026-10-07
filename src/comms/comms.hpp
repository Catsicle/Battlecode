#pragma once

#include "comm_function.hpp"

#include <cstdint>
#include <vector>



namespace bot::comms {

using DecodedSonar = std::vector<std::uint64_t>;

struct Inbox {
    std::vector<DecodedSonar> messages;
};

Inbox read_inbox(
    const unswbc::Controller& controller,
    std::uint64_t round
);

} // namespace bot::comms
