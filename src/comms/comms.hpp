#pragma once

#include "msg.hpp"

#include <array>
#include <vector>

namespace bot::comms {

struct Inbox {
    std::vector<RawMessage> messages;
    NavigationTarget navigation_target;
};

Inbox read_inbox(const unswbc::Controller& controller);

} // namespace bot::comms
