#include "executor.hpp"
#include "../roles/selector.hpp"

#include "safety.hpp"
#include "transition.hpp"
#include "../config.hpp"

#include <algorithm>
#include <deque>
#include <vector>

namespace bot::roles {
extern Role& queen_role();
} // namespace bot::roles

namespace bot::nav {

extern int reachable_area(const Grid& grid, int start, int body_size);

static bool valid_direction(Dir direction) {
    return direction.value == Dir::NORTH || direction.value == Dir::EAST ||
           direction.value == Dir::SOUTH || direction.value == Dir::WEST;
}

static Sim make_sim(const Memory& memory, const BodyTracker& tracker,
                    const unswbc::Controller& controller) {
    Sim sim;
    sim.stamp.assign(memory.cells, -1000000000);
    sim.pearl_taken.assign(memory.cells, 0);
    std::deque<int> body = tracker.body;
    if (body.empty())
        body.push_back(memory.index(controller.get_position()));
    sim.reset(body, controller.get_length());
    return sim;
}

static bool is_physically_non_colliding(const Memory& memory, const Grid& grid, Vec2 pos, Dir dir) {
    int cell = memory.index(pos);
    int d = dir_index(dir);
    if (cell < 0)
        return false;
    if (!grid.passable_edge(cell, d))
        return false;
    Vec2 next_pos = add(pos, dir, memory.width, memory.height);
    int next_cell = memory.index(next_pos);
    if (next_cell < 0)
        return false;

    Step step = resolve_step(memory, cell, d);
    if (step.kind == StepKind::Kelp || step.kind == StepKind::PortalUnresolved || step.dest < 0)
        return false;

    if (memory.occ_fresh(next_cell, grid.round)) {
        const Occupancy& occ = memory.occupancy[next_cell];
        if (occ.present)
            return false;
    }

    for (Vec2 body_tile : memory.own_body) {
        if (memory.index(body_tile) == next_cell)
            return false;
    }

    return true;
}

void execute_turn(unswbc::Controller& controller, Memory& memory, BodyTracker& tracker,
                  const Grid& grid, roles::Action& action, const bot::roles::Role* role) {
    if (action.request.mode == NavMode::Direct && !action.request.path.empty()) {
        std::vector<unswbc::Direction> moves;
        moves.reserve(action.request.path.size());
        for (Dir d : action.request.path) {
            moves.push_back(d);
        }
        controller.make_moves(moves);
        return;
    }

    if (action.request.mode == NavMode::Suicide) {
        Dir opposite = controller.get_dir().get_opposite();
        controller.make_move(opposite);
        return;
    }

    // Queen suicide split when blocked by ally body
    if (role != nullptr && role == &bot::roles::queen_role()) {
        bool all_blocked = true;
        Dir first_blocked_dir = Dir::NORTH;
        bool found_first_ally_blocked = false;
        for (Dir dir : directions()) {
            Vec2 current_pos = controller.get_position();
            bool blocked = !is_physically_non_colliding(memory, grid, current_pos, dir);
            if (!blocked) {
                all_blocked = false;
            }
            int next_cell = memory.index(add(current_pos, dir, memory.width, memory.height));
            bool blocked_by_ally = false;
            if (next_cell >= 0) {
                const auto& occ = memory.occupancy[next_cell];
                if (occ.present && occ.team == controller.get_team().value) {
                    blocked_by_ally = true;
                }
            }
            if (blocked_by_ally && !found_first_ally_blocked) {
                first_blocked_dir = dir;
                found_first_ally_blocked = true;
            }
        }
        if (all_blocked && controller.get_length() >= 4) {
            std::cout << "DEBUG: Queen blocked by ally all directions - split size=" << (controller.get_length() - 2) << std::endl;
            int child_size = controller.get_length() - 2;
            if (controller.can_split(child_size)) {
                uint64_t msg_round = static_cast<std::uint64_t>(grid.round);
                uint64_t suicide_msg = ::suicide_sonar(msg_round);
                action.sonar.North = (first_blocked_dir.value == 'N') ? suicide_msg : 0;
                action.sonar.East = (first_blocked_dir.value == 'E') ? suicide_msg : 0;
                action.sonar.South = (first_blocked_dir.value == 'S') ? suicide_msg : 0;
                action.sonar.West = (first_blocked_dir.value == 'W') ? suicide_msg : 0;
                controller.do_split(child_size);
                tracker.reset();
                return;
            }
        }
    }

    Sim initial = make_sim(memory, tracker, controller);
    std::uint8_t forbid = 0;
    int free_steps = 1;

    for (int attempt = 0; attempt < MAX_RETRY; ++attempt) {
        Path path = plan(action.request, grid, initial, controller.get_position(), forbid);
        if (!path.found || path.length == 0)
            break;
        int max_prefix = std::min(free_steps, path.length);
        for (int count = max_prefix; count >= 1; --count) {
            bool valid_path = true;
            for (int i = 0; i < count; ++i) {
                if (!valid_direction(path.steps[i])) {
                    valid_path = false;
                    break;
                }
            }
            if (!valid_path)
                continue;
            Sim sim = initial;
            Fail fail = Fail::OK;
            if (!validate_prefix(memory, path, count, sim, fail))
                continue;

            std::vector<Dir> moves(path.steps.begin(), path.steps.begin() + count);
            MoveRecord record;
            record.length_before = controller.get_length();
            record.traversed.reserve(count);
            record.pearl_known_before.reserve(count);
            Sim replay = initial;
            for (Dir move : moves) {
                Undo undo;
                if (replay.try_step(memory, dir_index(move), 0, undo) != Fail::OK)
                    break;
                record.traversed.push_back(replay.head);
                record.pearl_known_before.push_back(memory.pearl[replay.head]);
            }
            controller.make_move(moves.front());
            memory.apply_move_record(record, controller.get_length(), grid.round + 1);
            std::deque<int> predicted = tracker.body;
            if (predicted.empty())
                predicted.push_back(memory.index(controller.get_position()));
            replay = initial;
            for (Dir move : moves) {
                Undo undo;
                if (replay.try_step(memory, dir_index(move), 0, undo) != Fail::OK)
                    break;
                predicted.push_front(replay.head);
                while (static_cast<int>(predicted.size()) > replay.L)
                    predicted.pop_back();
            }
            tracker.predict(predicted);
            return;
        }
        forbid |= static_cast<std::uint8_t>(1U << dir_index(path.steps.front()));
    }

    // --- FALLBACK SELECTION ---
    bool found_cat1 = false;
    Dir best_cat1 = Dir::NORTH;
    int max_area_cat1 = -1;

    bool found_cat2 = false;
    Dir best_cat2 = Dir::NORTH;
    int max_area_cat2 = -1;

    bool found_cat3 = false;
    Dir best_cat3 = Dir::NORTH;
    int max_area_cat3 = -1;

    Vec2 current_pos = controller.get_position();

    for (Dir direction : directions()) {
        int d = dir_index(direction);
        Sim sim = initial;
        Undo undo;
        Fail fail = sim.try_step(memory, d, 0, undo);

        Vec2 next_pos = add(current_pos, direction, memory.width, memory.height);
        int next_cell = memory.index(next_pos);
        int area = (next_cell >= 0) ? reachable_area(grid, next_cell, 0) : 0;

        if (fail == Fail::OK) {
            if (!found_cat2 || area > max_area_cat2) {
                found_cat2 = true;
                best_cat2 = direction;
                max_area_cat2 = area;
            }

            if (trap_ok(memory, sim)) {
                if (!found_cat1 || area > max_area_cat1) {
                    found_cat1 = true;
                    best_cat1 = direction;
                    max_area_cat1 = area;
                }
            }
        }

        if (is_physically_non_colliding(memory, grid, current_pos, direction)) {
            if (!found_cat3 || area > max_area_cat3) {
                found_cat3 = true;
                best_cat3 = direction;
                max_area_cat3 = area;
            }
        }
    }

    if (found_cat1) {
        controller.make_move(best_cat1);
        return;
    }

    if (found_cat2) {
        controller.make_move(best_cat2);
        return;
    }

    if (found_cat3) {
        controller.make_move(best_cat3);
        return;
    }

    // Split ONLY when boxed in with zero non-colliding options left
    if (controller.can_split(controller.get_length() - 2)) {
        controller.do_split(controller.get_length() - 2);
        tracker.reset();
    } else {
        controller.make_move(controller.get_dir());
    }
}

} // namespace bot::nav