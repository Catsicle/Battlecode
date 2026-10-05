#pragma once

#include "../core/types.hpp"

#include <cstdint>

namespace bot::comms {

enum class MessageType : std::uint8_t {
    Invalid = 0,
    NavigateTarget = 1,
    Reserved2 = 2,
    Reserved3 = 3,
};

struct RawMessage {
    std::uint64_t bits = 0;
    MessageType type = MessageType::Invalid;
    std::uint8_t auth_placeholder = 0;
    bool valid = false;
};

struct NavigationTarget {
    bool available = false;
    Vec2 start{};
    Vec2 target{};
};

inline RawMessage decode_raw(std::uint64_t bits) {
    RawMessage result;
    result.bits = bits;
    result.type = static_cast<MessageType>(bits & 0x0fU);
    result.auth_placeholder = static_cast<std::uint8_t>((bits >> 4U) & 0x3fU);
    result.valid = result.type != MessageType::Invalid;
    return result;
}

// Coordinate and payload layout are intentionally deferred. This function is
// the only place that will need to change when the protocol is specified.
inline NavigationTarget decode_navigation_target(const RawMessage&) {
    return {};
}

} // namespace bot::comms
