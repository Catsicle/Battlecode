#include "../src/core/memory.hpp"
#include "../src/nav/sim.hpp"

#include <cassert>
#include <deque>

int main() {
    bot::Memory memory;
    memory.initialize(4, 4);
    for (int cell = 0; cell < memory.cells; ++cell) {
        memory.seen[cell] = true;
        memory.known_edges[cell] = 0x0f;
    }

    bot::MoveRecord record;
    record.length_before = 2;
    record.traversed = {1, 2, 3};
    record.pearl_known_before = {false, true, true};
    memory.pearl[2] = true;
    memory.pearl[3] = true;
    memory.apply_move_record(record, 3, 7);
    assert(memory.eaten_here[2]);
    assert(!memory.pearl[2]);
    assert(memory.visits_nopearl[1] == 1);
    assert(memory.last_visit_round[3] == 7);

    bot::nav::Sim sim;
    sim.stamp.assign(memory.cells, -1000000000);
    sim.pearl_taken.assign(memory.cells, 0);
    sim.reset(std::deque<int>{0, 1}, 2);
    assert(sim.seg_index(0) == 0);
    assert(sim.seg_index(1) == 1);
    bot::nav::Undo undo;
    assert(sim.try_step(memory, bot::dir_index(bot::Dir::EAST), 0, undo) ==
           bot::nav::Fail::SELF);

    bot::Memory enemy_memory;
    enemy_memory.initialize(8, 8);
    for (int cell = 0; cell < enemy_memory.cells; ++cell) {
        enemy_memory.seen[cell] = true;
        enemy_memory.known_edges[cell] = 0x0f;
    }
    enemy_memory.current_round = 5;
    enemy_memory.occupancy[2] = {true, true, 'B', 9, 5, -1, -1};
    bot::nav::Sim head_sim;
    head_sim.stamp.assign(enemy_memory.cells, -1000000000);
    head_sim.pearl_taken.assign(enemy_memory.cells, 0);
    head_sim.reset(std::deque<int>{0}, 1);
    assert(head_sim.try_step(enemy_memory, bot::dir_index(bot::Dir::EAST), 0, undo) ==
           bot::nav::Fail::HEAD_ON);
    return 0;
}
