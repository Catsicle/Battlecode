#pragma once

#include "../../core/memory.hpp"

#include <queue>
#include <vector>

namespace bot {

// Examine the chamber on the EXIT side of a portal.
//
// portal_id    : ID of the portal through which the bot entered.
// exit_portal  : coordinate of the portal cell on the other side.
//
// Returns:
//   {x, y}
//       An unknown tile was encountered. Explore it and call this function
//       again on the next turn with the same portal_id and exit_portal.
//
//   {1, xExit, yExit, xEnter, yEnter}
//       Chamber is SAFE: there are no additional portals and the reachable
//       chamber is fully bounded within depth 5 from the exit portal.
//
//   {-1, xExit, yExit}
//       Chamber is UNSAFE: another portal exists, or the chamber extends
//       beyond depth 5 through a known open edge.
inline std::vector<int> explore_portal(
    const Memory& memory,
    int portal_id,
    Vec2 exit_portal)
{
    constexpr int MAX_DEPTH = 5;

    const int exit_cell = memory.index(exit_portal);

    if (exit_cell < 0 || exit_cell >= memory.cells) {
        return {-1, exit_portal.x, exit_portal.y};
    }

    // Make sure the supplied exit coordinate actually belongs to portal_id.
    bool exit_portal_found = false;
    for (int dir = 0; dir < 4; ++dir) {
        if (memory.portal_id[exit_cell][dir] == portal_id) {
            exit_portal_found = true;
            break;
        }
    }

    if (!exit_portal_found) {
        return {-1, exit_portal.x, exit_portal.y};
    }

    // Find the other endpoint of the same portal.
    int enter_cell = -1;

    for (int cell = 0; cell < memory.cells; ++cell) {
        if (cell == exit_cell)
            continue;

        for (int dir = 0; dir < 4; ++dir) {
            if (memory.portal_id[cell][dir] == portal_id) {
                enter_cell = cell;
                break;
            }
        }

        if (enter_cell != -1)
            break;
    }

    if (enter_cell == -1) {
        return {-1, exit_portal.x, exit_portal.y};
    }

    const Vec2 enter_portal = memory.position(enter_cell);

    // BFS is restarted every turn. Memory is the persistent state.
    std::queue<std::pair<int, int>> q;
    std::vector<bool> visited(memory.cells, false);

    q.push({exit_cell, 0});
    visited[exit_cell] = true;

    while (!q.empty()) {
        auto [cell, depth] = q.front();
        q.pop();

        const Vec2 pos = memory.position(cell);

        // 0 = N, 1 = E, 2 = S, 3 = W
        static constexpr int dx[4] = {0, 1, 0, -1};
        static constexpr int dy[4] = {-1, 0, 1, 0};

        for (int dir = 0; dir < 4; ++dir) {
            // -------------------------------------------------
            // Unknown edge -> return the tile on the other side.
            // -------------------------------------------------
            if (!memory.edge_known(cell, dir)) {
                int nx = (pos.x + dx[dir]) % memory.width;
                int ny = (pos.y + dy[dir]) % memory.height;

                if (nx < 0) nx += memory.width;
                if (ny < 0) ny += memory.height;

                return {nx, ny};
            }

            // -------------------------------------------------
            // Portal edge.
            // -------------------------------------------------
            const int pid = memory.portal_id[cell][dir];

            if (pid != -1) {
                // The portal we entered through is the chamber boundary.
                // We do not traverse it.
                if (pid == portal_id)
                    continue;

                // Any other portal means the chamber is unsafe.
                return {-1, exit_portal.x, exit_portal.y};
            }

            // -------------------------------------------------
            // Kelp edge -> blocked, so nothing to explore there.
            // -------------------------------------------------
            if (memory.kelp[cell] & static_cast<std::uint8_t>(1U << dir))
                continue;

            // -------------------------------------------------
            // Known open edge.
            // -------------------------------------------------
            int nx = (pos.x + dx[dir]) % memory.width;
            int ny = (pos.y + dy[dir]) % memory.height;

            if (nx < 0) nx += memory.width;
            if (ny < 0) ny += memory.height;

            const Vec2 next{nx, ny};
            const int next_cell = memory.index(next);

            // Known open edge leading to an unknown tile.
            if (!memory.known(next_cell)) {
                return {nx, ny};
            }

            // Already reached through another path: this is fine.
            if (visited[next_cell])
                continue;

            // If depth 5 is the current boundary and there is a new
            // reachable open cell beyond it, the chamber is not fully
            // bounded within depth 5 -> UNSAFE.
            if (depth >= MAX_DEPTH) {
                return {-1, exit_portal.x, exit_portal.y};
            }

            visited[next_cell] = true;
            q.push({next_cell, depth + 1});
        }
    }

    // BFS finished without finding an unknown tile, an extra portal,
    // or a known open path beyond depth 5. Therefore the chamber is
    // fully bounded within depth 5 or less and is safe.
    return {
        1,
        exit_portal.x,
        exit_portal.y,
        enter_portal.x,
        enter_portal.y
    };
}

} // namespace bot
