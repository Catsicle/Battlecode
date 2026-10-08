#include "sim.hpp"
#include "../config.hpp"

#include <algorithm>
#include <cstdlib>

namespace bot::nav {

static int toroidal_distance(const Memory& memory, int first, int second) {
    Vec2 a = memory.position(first);
    Vec2 b = memory.position(second);
    int dx = std::abs(a.x - b.x);
    int dy = std::abs(a.y - b.y);
    dx = std::min(dx, memory.width - dx);
    dy = std::min(dy, memory.height - dy);
    return dx + dy;
}

void Sim::reset(const std::deque<int>& body, int length) {
    L = length;
    counter = 0;
    e = 0;
    head = body.empty() ? -1 : body.front();
    stamp.assign(stamp.size(), -1000000000);
    pearl_taken.assign(pearl_taken.size(), 0);
    for (std::size_t i = 0; i < body.size(); ++i)
        stamp[body[i]] = -static_cast<int>(i);
}

int Sim::seg_index(int tile) const {
    if (tile < 0 || tile >= static_cast<int>(stamp.size()) || stamp[tile] < -999999)
        return -1;
    int index = counter - stamp[tile];
    return index >= 0 && index < L ? index : -1;
}

Fail Sim::try_step(const Memory& memory, int direction, int rho, Undo& undo) {
    Step step = resolve_step(memory, head, direction);
    if (step.kind == StepKind::Kelp)
        return Fail::KELP;
    if (step.kind == StepKind::PortalUnresolved)
        return Fail::PORTAL_UNRESOLVED;
    if (step.dest < 0)
        return Fail::DEAD_END;
    if (seg_index(step.dest) >= 0)
        return Fail::SELF;
    if (memory.occ_fresh(step.dest, memory.current_round)) {
        const Occupancy& occupant = memory.occupancy[step.dest];
        if (occupant.head)
            return Fail::HEAD_ON;
        if (occupant.segment_index >= 0 && occupant.owner_length >= 0 &&
            rho < occupant.owner_length - occupant.segment_index)
            return Fail::OTHER_BODY;
        if (occupant.segment_index < 0 && rho == 0)
            return Fail::OTHER_BODY;
    }
    if (rho == 0) {
        for (int tile = 0; tile < memory.cells; ++tile) {
            const Occupancy& occupant = memory.occupancy[tile];
            if (!occupant.present || !occupant.head ||
                !memory.occ_fresh(tile, memory.current_round))
                continue;
            if (toroidal_distance(memory, step.dest, tile) <= DEFAULT_ENEMY_REACH)
                return Fail::HEAD_ON;
        }
    }
    undo = {step.dest, stamp[step.dest], false, head, L, e, counter};
    ++counter;
    stamp[step.dest] = counter;
    bool takes = memory.pearl[step.dest] && !pearl_taken[step.dest];
    if (takes) {
        pearl_taken[step.dest] = 1;
        ++L;
        ++e;
        undo.took_pearl = true;
    }
    head = step.dest;
    return Fail::OK;
}

void Sim::undo(const Undo& undo_state) {
    stamp[undo_state.tile] = undo_state.old_stamp;
    if (undo_state.took_pearl)
        pearl_taken[undo_state.tile] = 0;
    head = undo_state.old_head;
    L = undo_state.old_length;
    e = undo_state.old_e;
    counter = undo_state.old_counter;
}

} // namespace bot::nav
