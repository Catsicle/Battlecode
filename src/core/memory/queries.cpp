#include "queries.hpp"
#include "../memory.hpp"
#include <optional>

namespace bot {

std::pair<std::optional<int>, std::optional<int>> get_cell(const Memory& memory, int x, int y) {
    int w = memory.width;
    int h = memory.height;
    if (w <= 0 || h <= 0 || x < 0 || x >= w || y < 0 || y >= h) {
        return {std::nullopt, std::nullopt};
    }
    int cell = memory.index({x, y});
    if (cell < 0 || cell >= memory.cells) {
        return {std::nullopt, std::nullopt};
    }
    bool visited = memory.seen[cell];
    bool has_pearl = memory.pearl[cell];
    int first = -1;
    if (!visited) {
        first = -1;
    } else {
        const auto& occ = memory.occupancy[cell];
        bool is_own = false;
        for (auto const& pos : memory.own_body) {
            if (memory.index(pos) == cell) {
                is_own = true;
                break;
            }
        }
        if (is_own) {
            first = occ.head ? 2 : 1;
        } else if (occ.present) {
            bool same_team = occ.team == memory.my_team.value;
            if (same_team) {
                first = occ.head ? 4 : 3; // ally/ally_head
            } else {
                first = occ.head ? 6 : 5; // enemy/enemy_head
            }
        } else {
            first = 0; // empty
        }
    }
    int second = has_pearl ? 1 : 0;
    return {std::optional<int>(first), std::optional<int>(second)};
}

std::pair<std::optional<int>, std::optional<int>> get_edge(const Memory& memory, int x, int y, int dir) {
    int w = memory.width;
    int h = memory.height;
    if (w <= 0 || h <= 0 || x < 0 || x >= w || y < 0 || y >= h) {
        return {std::nullopt, std::nullopt};
    }
    int cell = memory.index({x, y});
    if (cell < 0 || cell >= memory.cells) {
        return {std::nullopt, std::nullopt};
    }
    bool known_edge = (memory.known_edges[cell] & (1U << dir)) != 0;
    int first = 0;
    int second = 0;
    if (!memory.seen[cell]) {
        first = -1; // unknown
        second = 0;
    } else if (!known_edge) {
        first = -1; // unknown edge
        second = 0;
    } else {
        bool is_kelp = (memory.kelp[cell] & (1U << dir)) != 0;
        if (is_kelp) {
            first = 1;
            second = 0;
        } else {
            int pid = memory.portal_id[cell][dir];
            if (pid >= 0) {
                first = 2;
                second = pid;
            } else {
                first = 0; // empty
                second = 0;
            }
        }
    }
    return {std::optional<int>(first), std::optional<int>(second)};
}

} // namespace bot
