#include "comms.hpp"

namespace bot::comms {

Inbox read_inbox(const unswbc::Controller& controller) {
    Inbox inbox;
    for (std::uint64_t bits : controller.get_sonar_messages()) {
        RawMessage message = decode_raw(bits);
        if (!message.valid)
            continue;
        inbox.messages.push_back(message);
        if (message.type == MessageType::NavigateTarget && !inbox.navigation_target.available)
            inbox.navigation_target = decode_navigation_target(message);
    }
    return inbox;
}

} // namespace bot::comms
