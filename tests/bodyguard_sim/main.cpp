#include "bodyguard_sim/sim.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

using bodyguard_sim::Action;
using bodyguard_sim::DeathReason;
using bodyguard_sim::Direction;
using bodyguard_sim::World;

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void test_wraps() {
    World world;
    world.initialize(4, 3);
    world.add_dragon(0, world.cell_at(0, 0));
    Action action;
    action.push(Direction::North);
    require(world.move(0, action) == DeathReason::None, "north wrap should survive");
    require(world.head(0) == world.cell_at(0, 2), "north wrap destination");
}

void test_kelp_and_body_collisions() {
    World world;
    world.initialize(4, 3);
    world.add_dragon(0, world.cell_at(1, 1));
    world.set_kelp(world.cell_at(1, 1), Direction::East);
    Action kelp;
    kelp.push(Direction::East);
    require(world.move(0, kelp) == DeathReason::Wall, "kelp should kill");

    world.initialize(4, 3);
    std::array<int, bodyguard_sim::MAX_CELLS> body{};
    body[0] = world.cell_at(1, 1);
    body[1] = world.cell_at(0, 1);
    world.add_dragon(0, body, 2);
    Action self;
    self.push(Direction::West);
    require(world.move(0, self) == DeathReason::Self, "own tail should be blocked");
}

void test_pearl_growth_and_paid_step() {
    World world;
    world.initialize(5, 2);
    world.add_dragon(0, world.cell_at(0, 0));
    world.set_pearl(world.cell_at(1, 0));
    Action pearl;
    pearl.push(Direction::East);
    require(world.move(0, pearl) == DeathReason::None, "pearl move should survive");
    require(world.length(0) == 2 && !world.has_pearl(world.cell_at(1, 0)), "pearl should grow dragon");

    world.initialize(5, 2);
    world.add_dragon(0, world.cell_at(0, 0));
    Action sprint;
    sprint.push(Direction::East);
    sprint.push(Direction::East);
    require(world.move(0, sprint) == DeathReason::NoValidAction, "length two cannot pay");
    require(!world.alive(0), "unpaid sprint should kill");
}

void test_head_to_head() {
    World world;
    world.initialize(4, 2);
    world.add_dragon(0, world.cell_at(0, 0));
    world.add_dragon(1, world.cell_at(1, 0));
    Action action;
    action.push(Direction::East);
    require(world.move(0, action) == DeathReason::HeadToHead, "head collision reason");
    require(!world.alive(0) && !world.alive(1), "head collision kills both");
}

void test_splitting_and_death_drops() {
    World world;
    world.initialize(8, 2);
    std::array<int, bodyguard_sim::MAX_CELLS> body{};
    body[0] = world.cell_at(1, 0);
    body[1] = world.cell_at(2, 0);
    body[2] = world.cell_at(3, 0);
    body[3] = world.cell_at(4, 0);
    body[4] = world.cell_at(5, 0);
    body[5] = world.cell_at(6, 0);
    world.add_dragon(0, body, 6);
    require(world.split(0, 2), "valid split should succeed");
    require(world.alive(0) && world.alive(1), "split should leave both dragons alive");
    require(world.length(0) == 4 && world.length(1) == 2, "split lengths");
    require(world.head(1) == world.cell_at(6, 0), "child starts at old tail");
    require(world.next_unused_id() == 2, "split child id");

    world.initialize(8, 2);
    world.add_dragon(0, body, 6);
    require(!world.split(0, 1), "length-one child split should fail");
    require(!world.alive(0), "invalid split should kill parent");

    world.initialize(8, 2);
    world.add_dragon(0, body, 6);
    world.set_kelp(world.cell_at(1, 0), Direction::East);
    Action action;
    action.push(Direction::East);
    require(world.move(0, action) == DeathReason::Wall, "death-drop setup");
    require(world.has_pearl(world.cell_at(1, 0)) && world.has_pearl(world.cell_at(3, 0)) && world.has_pearl(world.cell_at(5, 0)), "death drops every second segment");
}

} // namespace

void run_filter(std::string_view filter) {
    if (filter == "all" || filter == "movement") {
        test_wraps();
        test_kelp_and_body_collisions();
        test_pearl_growth_and_paid_step();
    }
    if (filter == "all" || filter == "collisions")
        test_head_to_head();
    if (filter == "all" || filter == "splitting")
        test_splitting_and_death_drops();
}

int main(int argc, char** argv) {
    std::string_view filter = "all";
    if (argc == 3 && std::string_view(argv[1]) == "--filter")
        filter = argv[2];
    if (filter != "all" && filter != "movement" && filter != "collisions" && filter != "splitting" && filter != "sonar") {
        std::cerr << "unknown simulator filter\n";
        return 2;
    }
    if (filter == "sonar") {
        std::cout << "no tests implemented for " << filter << " yet\n";
        return 0;
    }
    run_filter(filter);
    std::cout << "bodyguard simulator tests passed\n";
    return 0;
}
