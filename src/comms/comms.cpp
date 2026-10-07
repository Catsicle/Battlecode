#include "comms.hpp"
#include "../helper.hpp"
#include <algorithm>

namespace bot::comms {

Inbox read_inbox(
    const unswbc::Controller& controller,
    std::uint64_t round)
{
    Inbox inbox;

    for (std::uint64_t bits : controller.get_sonar_messages()) {
        std::vector<std::uint64_t> decoded = decode_sonar(bits, round);

        if (decoded.empty())
            continue;

        std::uint8_t sonar_id = static_cast<std::uint8_t>(decoded[0]);

        if (sonar_id == 0b1111) {
            // Enemy Queen
            enemy_queen_sonar msg{};
            msg.x = static_cast<std::uint8_t>(decoded[1]);
            msg.y = static_cast<std::uint8_t>(decoded[2]);
            msg.round = round;
            inbox.enemy_queen_messages.push_back(msg);
            inbox.enemy_queen_exists = true;
        }
        else if (sonar_id == 0b0010) {
            // Idle
            idle_sonar msg{};
            msg.hx = static_cast<std::uint8_t>(decoded[1]);
            msg.hy = static_cast<std::uint8_t>(decoded[2]);
            msg.pearl_grid.reserve(36);
            for (std::size_t i = 3; i < decoded.size(); ++i) {
                msg.pearl_grid.push_back(static_cast<bool>(decoded[i]));
            }
            msg.round = round;
            inbox.idle_messages.push_back(msg);
            inbox.idle_exists = true;
        }
        else if (sonar_id == 0b0001) {
            // Portal
            portal_sonar msg{};
            msg.sx = static_cast<std::uint8_t>(decoded[1]);
            msg.sy = static_cast<std::uint8_t>(decoded[2]);
            msg.sdir = static_cast<std::uint8_t>(decoded[3]);
            msg.x1 = static_cast<std::uint8_t>(decoded[4]);
            msg.y1 = static_cast<std::uint8_t>(decoded[5]);
            msg.x2 = static_cast<std::uint8_t>(decoded[6]);
            msg.y2 = static_cast<std::uint8_t>(decoded[7]);
            msg.dir2 = static_cast<std::uint8_t>(decoded[8]);
            msg.x3 = static_cast<std::uint8_t>(decoded[9]);
            msg.y3 = static_cast<std::uint8_t>(decoded[10]);
            msg.dir3 = static_cast<std::uint8_t>(decoded[11]);
            msg.round = round;
            inbox.portal_messages.push_back(msg);
            inbox.portal_exists = true;
        }
        else if (sonar_id == 0b0000) {
            // Suicide
            suicide_sonar msg{};
            msg.secret = decoded[1];
            msg.round = round;
            inbox.suicide_messages.push_back(msg);
            inbox.suicide_exists = true;
        }
        else if (sonar_id == 0b0011) {
            // Queen killed
            queen_killed_sonar msg{};
            msg.round = round;
            inbox.queen_killed_messages.push_back(msg);
            inbox.queen_killed_exists = true;
        }
        else if (sonar_id == 0b1001) {
            // Food signal
            food_signal_sonar msg{};
            msg.x = static_cast<std::uint8_t>(decoded[1]);
            msg.y = static_cast<std::uint8_t>(decoded[2]);
            msg.value = static_cast<std::uint8_t>(decoded[3]);
            msg.round = round;
            inbox.food_signal_messages.push_back(msg);
            inbox.food_signal_exists = true;
        }
    }

    // Sort each message vector by round descending
    std::sort(inbox.enemy_queen_messages.begin(), inbox.enemy_queen_messages.end(),
              [](const enemy_queen_sonar& a, const enemy_queen_sonar& b) { return a.round > b.round; });
    std::sort(inbox.idle_messages.begin(), inbox.idle_messages.end(),
              [](const idle_sonar& a, const idle_sonar& b) { return a.round > b.round; });
    std::sort(inbox.portal_messages.begin(), inbox.portal_messages.end(),
              [](const portal_sonar& a, const portal_sonar& b) { return a.round > b.round; });
    std::sort(inbox.suicide_messages.begin(), inbox.suicide_messages.end(),
              [](const suicide_sonar& a, const suicide_sonar& b) { return a.round > b.round; });
    std::sort(inbox.queen_killed_messages.begin(), inbox.queen_killed_messages.end(),
              [](const queen_killed_sonar& a, const queen_killed_sonar& b) { return a.round > b.round; });
    std::sort(inbox.food_signal_messages.begin(), inbox.food_signal_messages.end(),
              [](const food_signal_sonar& a, const food_signal_sonar& b) { return a.round > b.round; });

    return inbox;
}

} // namespace bot::comms
