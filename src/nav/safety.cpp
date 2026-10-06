#include "safety.hpp"
#include "../config.hpp"

#include <queue>

namespace bot::nav {

bool trap_ok(const Memory& memory, const Sim& sim) {
    if (sim.head < 0)
        return false;
    std::vector<bool> seen(memory.cells, false);
    std::queue<int> q;
    q.push(sim.head);
    seen[sim.head] = true;
    int area = 0;
    bool frontier = false;
    while (!q.empty()) {
        int tile = q.front();
        q.pop();
        ++area;
        if (memory.is_frontier(tile))
            frontier = true;
        for (int d = 0; d < 4; ++d) {
            Step step = resolve_step(memory, tile, d);
            if (step.dest < 0 || step.kind == StepKind::Kelp ||
                step.kind == StepKind::PortalUnresolved || seen[step.dest])
                continue;
            if (sim.seg_index(step.dest) >= 0)
                continue;
            if (memory.occ_fresh(step.dest, memory.current_round))
                continue;
            seen[step.dest] = true;
            q.push(step.dest);
        }
    }
    return area >= sim.L + sim.e + TRAP_EXTRA || frontier;
}

bool survives(const Memory& memory, Sim& sim, int depth) {
    if (depth <= 0)
        return true;
    for (int d = 0; d < 4; ++d) {
        Undo undo;
        if (sim.try_step(memory, d, 0, undo) != Fail::OK)
            continue;
        bool okay = survives(memory, sim, depth - 1);
        sim.undo(undo);
        if (okay)
            return true;
    }
    return false;
}

bool validate_prefix(const Memory& memory, const Path& path, int count, Sim& sim, Fail& fail) {
    for (int i = 0; i < count; ++i) {
        Undo undo;
        fail = sim.try_step(memory, dir_index(path.steps[i]), i / std::max(1, (sim.L + 3) / 4), undo);
        if (fail != Fail::OK)
            return false;
    }
    if (!trap_ok(memory, sim)) {
        fail = Fail::TRAP;
        return false;
    }
    if (!survives(memory, sim, LOOKAHEAD_K)) {
        fail = Fail::DEAD_END;
        return false;
    }
    fail = Fail::OK;
    return true;
}

} // namespace bot::nav
