#include "memory.hpp"
#include "../config.hpp"

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
    portal_ends.assign(256, {});
}

void Memory::update(const unswbc::Controller& controller, int round) {
    current_round = round;
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
            if (part->get_id() == controller.get_id()) {
                own_body.push_back(tile.get_position());
            } else {
                occupancy[cell] = {true, part->is_head(), part->get_team().value,
                                   part->get_id(), round, -1, -1};
            }
        }

        for (Dir direction : directions()) {
            int d = dir_index(direction);
            known_edges[cell] |= static_cast<std::uint8_t>(1U << d);
            auto const& edge = tile.get_edge(direction);
            if (edge.get_edge_type() == unswbc::EdgeType::KELP)
                kelp[cell] |= static_cast<std::uint8_t>(1U << d);
            if (edge.get_edge_type() == unswbc::EdgeType::PORTAL)
            {
                int id = edge.get_portal_id();
                portal_id[cell][d] = static_cast<std::int16_t>(id);
                if (id >= static_cast<int>(portal_ends.size()))
                    portal_ends.resize(id + 1, {});
                PortalEnds& ends = portal_ends[id];
                if (ends.count == 0 || ends.tile[0] != cell) {
                    if (ends.count == 0) {
                        ends.tile[0] = cell;
                        ends.direction[0] = static_cast<std::uint8_t>(d);
                        ends.count = 1;
                    } else if (ends.count == 1 && ends.tile[1] != cell) {
                        ends.tile[1] = cell;
                        ends.direction[1] = static_cast<std::uint8_t>(d);
                        ends.count = 2;
                    }
                }
            }
        }
    }

}

void Memory::apply_move_record(const MoveRecord& record, int length_now, int round) {
    int eaten = std::max(0, length_now - record.length_before);
    for (std::size_t i = 0; i < record.traversed.size(); ++i) {
        int cell = record.traversed[i];
        if (cell < 0 || cell >= cells)
            continue;
        bool pearl = i < record.pearl_known_before.size() && record.pearl_known_before[i];
        if (pearl && eaten > 0) {
            eaten_here[cell] = true;
            this->pearl[cell] = false;
            --eaten;
        } else {
            ++visits_nopearl[cell];
        }
        last_visit_round[cell] = round;
    }
}

bool Memory::occ_fresh(int cell, int now) const {
    return cell >= 0 && cell < cells && occupancy[cell].present &&
           now - occupancy[cell].round <= OCC_STALE_ROUNDS;
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
