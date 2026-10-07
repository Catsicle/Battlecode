#include "comms.hpp"
#include "../helper.hpp"
// Include the project header that provides the full definition of
// unswbc::Controller before compiling this file. The uploaded comms package
// does not contain that project header, so its exact path cannot be inferred.

namespace bot::comms {

Inbox read_inbox(
    const unswbc::Controller& controller,
    std::uint64_t round)
{
    Inbox inbox;

    for (std::uint64_t bits : controller.get_sonar_messages()) {
        DecodedSonar decoded = decode_sonar(bits, round);

        if (!decoded.empty())
            if(decoded.size()>0)
            inbox.messages.push_back(decoded);
    }

    return inbox;
}

} // namespace bot::comms
