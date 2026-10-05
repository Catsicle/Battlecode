#include "memory.hpp"

#include <algorithm>

namespace bot {

void Memory::initialize(int map_width, int map_height) {
    width = map_width;
    height = map_height;
    cells = width * height;
    kelp.assign(cells, 0);
    known_edges.assign(cells, 0);
    portal_id.assign(cells, {-1, -1, -1, -1});
    portal_exit_known.assign(cells, {false, false, false, false});
    seen.assign(cells, false);
    last_seen_round.assign(cells, -1);
    pearl_spawn_round.assign(cells, std::numeric_limits<int>::max());
    never_spawns.assign(cells, false);
    visits_nopearl.assign(cells, 0);
    last_visit_round.assign(cells, -1);
    eaten_here.assign(cells, false);
    occupancy.assign(cells, {});
    pearl.assign(cells, false);
    own_body.clear();
    previous_head = {};
    has_previous_head = false;
    previous_move_ate = false;
}

void Memory::update(const unswbc::Controller& controller, int round) {
    struct PortalEndpoint {
        int cell;
        int direction;
        int id;
    };
    std::vector<PortalEndpoint> visible_portals;
    Vec2 current_head = controller.get_position();
    if (has_previous_head && current_head != previous_head) {
        int cell = index(current_head);
        bool ate = cell >= 0 && cell < cells && controller.get_tile(current_head) != nullptr
                   && controller.get_tile(current_head)->has_pearl();
        previous_move_ate = ate;
        if (cell >= 0 && cell < cells) {
            last_visit_round[cell] = round;
            if (!ate) {
                ++visits_nopearl[cell];
            } else {
                eaten_here[cell] = true;
            }
        }
    }
    previous_head = current_head;
    has_previous_head = true;

    my_team = controller.get_team();
    own_body.clear();
    for (auto const& tile : controller.get_tiles()) {
        int cell = index(tile.get_position());
        if (cell < 0 || cell >= cells)
            continue;
        seen[cell] = true;
        last_seen_round[cell] = round;
        pearl[cell] = tile.has_pearl();
        int countdown = tile.get_pearl_time();
        if (countdown < 0) {
            never_spawns[cell] = true;
            pearl_spawn_round[cell] = std::numeric_limits<int>::max();
        } else {
            pearl_spawn_round[cell] = round + countdown;
        }
        auto const* part = tile.get_dragon();
        occupancy[cell] = {};
        if (part != nullptr) {
            occupancy[cell] = {true, part->is_head(), part->get_team().value, part->get_id(), round};
            if (part->get_id() == controller.get_id())
                own_body.push_back(tile.get_position());
        }
        for (Dir direction : directions()) {
            int d = dir_index(direction);
            known_edges[cell] |= static_cast<std::uint8_t>(1U << d);
            auto const& edge = tile.get_edge(direction);
            if (edge.get_edge_type() == unswbc::EdgeType::KELP)
                kelp[cell] |= static_cast<std::uint8_t>(1U << d);
            if (edge.get_edge_type() == unswbc::EdgeType::PORTAL)
                portal_id[cell][d] = static_cast<std::int16_t>(edge.get_portal_id());
            if (edge.get_edge_type() == unswbc::EdgeType::PORTAL)
                visible_portals.push_back({cell, d, edge.get_portal_id()});
        }
    }

    for (const PortalEndpoint& endpoint : visible_portals) {
        for (const PortalEndpoint& candidate : visible_portals) {
            if (candidate.id == endpoint.id && (candidate.cell != endpoint.cell || candidate.direction != endpoint.direction)) {
                portal_exit_known[endpoint.cell][endpoint.direction] = true;
                break;
            }
        }
    }
}

bool Memory::known(int cell) const {
    return cell >= 0 && cell < cells && seen[cell];
}

bool Memory::edge_known(int cell, int direction) const {
    return cell >= 0 && cell < cells && (known_edges[cell] & (1U << direction)) != 0;
}

bool Memory::is_frontier(int cell) const {
    if (!known(cell))
        return false;
    Vec2 p = position(cell);
    for (Dir direction : directions()) {
        if (!known(index(add(p, direction, width, height))))
            return true;
    }
    return false;
}

} // namespace bot
