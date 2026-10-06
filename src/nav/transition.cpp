#include "transition.hpp"

namespace bot::nav {

Step resolve_step(const Memory& memory, int tile, int direction) {
    if (!memory.edge_known(tile, direction))
        return {-1, static_cast<std::uint8_t>(direction), StepKind::PortalUnresolved};
    if ((memory.kelp[tile] & (1U << direction)) != 0)
        return {-1, static_cast<std::uint8_t>(direction), StepKind::Kelp};
    int portal = memory.portal_id[tile][direction];
    Vec2 position = memory.position(tile);
    Dir dir = directions()[direction];
    if (portal < 0)
        return {memory.index(add(position, dir, memory.width, memory.height)),
                static_cast<std::uint8_t>(direction), StepKind::Normal};
    if (portal >= static_cast<int>(memory.portal_ends.size()) ||
        memory.portal_ends[portal].count != 2)
        return {-1, static_cast<std::uint8_t>(direction), StepKind::PortalUnresolved};
    const PortalEnds& ends = memory.portal_ends[portal];
    int endpoint = ends.tile[0] == tile ? 1 : 0;
    int exit_tile = ends.tile[endpoint];
    int exit_direction = ends.direction[endpoint];
    if (!memory.edge_known(exit_tile, exit_direction))
        return {-1, static_cast<std::uint8_t>(direction), StepKind::PortalUnresolved};
    if ((memory.kelp[exit_tile] & (1U << exit_direction)) != 0)
        return {-1, static_cast<std::uint8_t>(direction), StepKind::Kelp};
    Vec2 emergence = add(memory.position(exit_tile), directions()[exit_direction],
                         memory.width, memory.height);
    return {memory.index(emergence), static_cast<std::uint8_t>(exit_direction),
            StepKind::PortalOK};
}

} // namespace bot::nav
