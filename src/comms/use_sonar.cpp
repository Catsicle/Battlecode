#include "use_sonar.hpp"
#include "../core/memory.hpp"

namespace bot::comms {

std::uint8_t compute_propagate(const Inbox& inbox) {
    std::uint8_t propagate_val = 3;
    if (inbox.enemy_queen_exists) {
        std::uint8_t max_prop = 0;
        for (auto msg : inbox.enemy_queen_messages) {
            if (msg.propagate > max_prop) max_prop = msg.propagate;
        }
        propagate_val = (max_prop > 0) ? (max_prop - 1) : 0;
    }
    return propagate_val;
}

std::uint64_t maybe_emit_enemy_queen_sonar(
    const unswbc::Controller& controller,
    const Inbox& inbox,
    std::uint64_t round_num) {
    Vec2 queen_spot;
    bool queen_spotted = false;
    for (const auto& tile : controller.get_vision().get_tiles()) {
        const auto* dragon = tile.get_dragon();
        if (dragon != nullptr && dragon->is_head() &&
            (dragon->get_id() == 0 || dragon->get_id() == 1) &&
            dragon->get_team() != controller.get_team()) {
            queen_spotted = true;
            queen_spot = tile.get_position();
            break;
        }
    }
    if (!queen_spotted)
        return 0;
    std::uint8_t propagate_val = compute_propagate(inbox);
    return ::enemy_queen_sonar(
        static_cast<std::uint8_t>(queen_spot.x & 0x3F),
        static_cast<std::uint8_t>(queen_spot.y & 0x3F),
        propagate_val,
        round_num);
}

std::uint64_t maybe_emit_queen_beacon_sonar(
    const unswbc::Controller& controller,
    const Inbox& inbox,
    std::uint64_t round_num)
{
    (void)inbox;
    printf("DEBUG: maybe_emit_queen_beacon_sonar called round=%lu\n", (unsigned long)round_num);
    bool queen_visible = false;
    Vec2 queen_spot{};
    for (const auto& tile : controller.get_vision().get_tiles()) {
        const auto* dragon = tile.get_dragon();
        if (dragon != nullptr && dragon->is_head() &&
            (dragon->get_id() == 0 || dragon->get_id() == 1) &&
            dragon->get_team() == controller.get_team()) {
            printf("DEBUG: beacon queen visible at (%d,%d)\n", (int)tile.get_position().x, (int)tile.get_position().y);
            queen_visible = true;
            queen_spot = tile.get_position();
            break;
        }
    }
    if (!queen_visible) {
        printf("DEBUG: beacon no queen visible\n");
        return 0;
    }
    std::uint64_t result = ::queen_beacon_sonar(
        static_cast<std::uint8_t>(queen_spot.x & 0x3F),
        static_cast<std::uint8_t>(queen_spot.y & 0x3F),
        round_num);
    printf("DEBUG: beacon emitted sonar=%lu for queen at (%d,%d)\n",
           (unsigned long)result, (int)queen_spot.x, (int)queen_spot.y);
    return result;
}

} // namespace bot::comms