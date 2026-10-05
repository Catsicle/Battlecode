#include "bodyguard_sim/sim.hpp"
#include "bodyguard_sim/trace.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

using bodyguard_sim::Action;
using bodyguard_sim::Cell;
using bodyguard_sim::TurnAction;
using bodyguard_sim::DeathReason;
using bodyguard_sim::Direction;
using bodyguard_sim::TraceScript;
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

    world.initialize(5, 2);
    world.add_dragon(0, world.cell_at(0, 0));
    world.set_pearl_countdown(world.cell_at(1, 0), 1);
    std::array<TurnAction, bodyguard_sim::MAX_DRAGONS> actions{};
    Action timed_move;
    timed_move.push(Direction::East);
    actions[0] = TurnAction::move(timed_move);
    world.run_round(actions);
    require(world.length(0) == 2, "pearl must tick before movement");
}

void test_portal_traversal() {
    World world;
    world.initialize(6, 2);
    Cell first = world.cell_at(0, 0);
    Cell second = world.cell_at(3, 0);
    world.add_dragon(0, first);
    world.set_portal(first, Direction::East, second, Direction::East);
    Action action;
    action.push(Direction::East);
    require(world.move(0, action) == DeathReason::None, "portal move should survive");
    require(world.head(0) == world.cell_at(4, 0), "portal should emerge beyond paired edge");
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

    world.initialize(8, 2);
    world.add_dragon(0, body, 6);
    std::array<TurnAction, bodyguard_sim::MAX_DRAGONS> actions{};
    actions[0] = TurnAction::split(2);
    Action child_move;
    child_move.push(Direction::East);
    actions[1] = TurnAction::move(child_move);
    world.run_round(actions);
    require(world.alive(1) && world.head(1) == world.cell_at(7, 0), "split child should act later in the same round");
}

void test_sonar() {
    World world;
    world.initialize(8, 3);
    world.add_dragon(0, world.cell_at(0, 1), 'A');
    world.add_dragon(1, world.cell_at(3, 0), 'A');
    std::array<TurnAction, bodyguard_sim::MAX_DRAGONS> actions{};
    Action sender_action;
    sender_action.push(Direction::North);
    sender_action.send_sonar(Direction::East, 123);
    actions[0] = TurnAction::move(sender_action);
    Action receiver_action;
    receiver_action.push(Direction::North);
    actions[1] = TurnAction::move(receiver_action);
    world.run_round(actions);
    require(world.inbox(1).count == 1 && world.inbox(1).messages[0] == 123, "higher ID receives sonar this round");
    require(world.echoes(0).count(bodyguard_sim::SonarHit::AllyHead) == 1, "ally head echo");

    world.initialize(8, 3);
    world.add_dragon(0, world.cell_at(3, 1), 'A');
    world.add_dragon(1, world.cell_at(0, 1), 'B');
    actions = {};
    Action enemy_sender;
    enemy_sender.push(Direction::East);
    enemy_sender.send_sonar(Direction::East, 456);
    actions[1] = TurnAction::move(enemy_sender);
    Action enemy_target;
    enemy_target.push(Direction::East);
    actions[0] = TurnAction::move(enemy_target);
    world.run_round(actions);
    require(world.inbox(0).count == 0, "lower ID waits for next round");
    require(world.echoes(1).count(bodyguard_sim::SonarHit::EnemyHead) == 1, "enemy head echo");
    world.run_round(actions);
    require(world.inbox(0).count == 1 && world.inbox(0).messages[0] == 456, "lower ID receives sonar next round");

    world.initialize(8, 3);
    world.add_dragon(0, world.cell_at(0, 1));
    world.set_kelp(world.cell_at(0, 0), Direction::East);
    actions = {};
    Action kelp_sender;
    kelp_sender.push(Direction::North);
    kelp_sender.send_sonar(Direction::East, 789);
    actions[0] = TurnAction::move(kelp_sender);
    world.run_round(actions);
    require(world.echoes(0).count(bodyguard_sim::SonarHit::Kelp) == 1, "kelp stops sonar");

    world.initialize(8, 3);
    world.add_dragon(0, world.cell_at(0, 1));
    world.add_dragon(1, world.cell_at(5, 0));
    world.set_portal(world.cell_at(0, 0), Direction::East, world.cell_at(4, 0), Direction::East);
    actions = {};
    Action portal_sender;
    portal_sender.push(Direction::North);
    portal_sender.send_sonar(Direction::East, 999);
    actions[0] = TurnAction::move(portal_sender);
    actions[1] = TurnAction::move(receiver_action);
    world.run_round(actions);
    require(world.inbox(1).count == 1 && world.inbox(1).messages[0] == 999, "sonar crosses portal");
}

void test_deterministic_trace_batch() {
    for (std::uint32_t seed = 0; seed < 50; ++seed) {
        World first;
        World second;
        first.initialize(11, 7);
        second.initialize(11, 7);
        first.add_dragon(0, first.cell_at(1, 1), 'A');
        second.add_dragon(0, second.cell_at(1, 1), 'A');
        first.add_dragon(1, first.cell_at(9, 5), 'B');
        second.add_dragon(1, second.cell_at(9, 5), 'B');
        TraceScript script{seed, 24};
        for (int round = 0; round < script.rounds; ++round) {
            std::array<TurnAction, bodyguard_sim::MAX_DRAGONS> first_actions{};
            std::array<TurnAction, bodyguard_sim::MAX_DRAGONS> second_actions{};
            first_actions[0] = script.action(round, 0);
            first_actions[1] = script.action(round, 1);
            second_actions[0] = script.action(round, 0);
            second_actions[1] = script.action(round, 1);
            first.run_round(first_actions);
            second.run_round(second_actions);
            require(first.state_hash() == second.state_hash(), "deterministic trace diverged");
        }
    }
}

int state_trace(int argc, char** argv) {
    std::uint32_t seed = 0;
    std::vector<int> initial_pearls;
    std::vector<std::pair<int, int>> scheduled_pearls;
    for (int index = 2; index < argc; ++index) {
        if (std::string_view(argv[index]) == "--seed" && index + 1 < argc)
            seed = static_cast<std::uint32_t>(std::strtoul(argv[++index], nullptr, 10));
        else if (std::string_view(argv[index]) == "--pearl" && index + 1 < argc)
            initial_pearls.push_back(std::atoi(argv[++index]));
        else if (std::string_view(argv[index]) == "--spawn" && index + 1 < argc) {
            std::string value = argv[++index];
            std::size_t separator = value.find(':');
            if (separator != std::string::npos)
                scheduled_pearls.emplace_back(std::stoi(value.substr(0, separator)), std::stoi(value.substr(separator + 1)));
        }
    }
    World world;
    world.initialize(64, 64);
    std::array<std::array<int, 6>, 6> bodies{{
        {{13 + 64 * 14, 14 + 64 * 14, 15 + 64 * 14, 16 + 64 * 14, 17 + 64 * 14, 18 + 64 * 14}},
        {{50 + 64 * 49, 49 + 64 * 49, 48 + 64 * 49, 47 + 64 * 49, 46 + 64 * 49, 45 + 64 * 49}},
        {{18 + 64 * 17, 19 + 64 * 17, 20 + 64 * 17, 21 + 64 * 17, 22 + 64 * 17, 23 + 64 * 17}},
        {{45 + 64 * 46, 44 + 64 * 46, 43 + 64 * 46, 42 + 64 * 46, 41 + 64 * 46, 40 + 64 * 46}},
        {{8 + 64 * 11, 9 + 64 * 11, 10 + 64 * 11, 11 + 64 * 11, 12 + 64 * 11, 13 + 64 * 11}},
        {{55 + 64 * 52, 54 + 64 * 52, 53 + 64 * 52, 52 + 64 * 52, 51 + 64 * 52, 50 + 64 * 52}},
    }};
    for (int dragon_id = 0; dragon_id < 6; ++dragon_id) {
        std::array<int, bodyguard_sim::MAX_CELLS> body{};
        for (int index = 0; index < 6; ++index)
            body[index] = bodies[dragon_id][index];
        world.add_dragon(dragon_id, body, 6, dragon_id % 2 == 0 ? 'A' : 'B');
    }

    for (int pearl : initial_pearls)
        world.set_pearl(pearl);
    TraceScript script{seed, 24};
    for (int round = 0; round < script.rounds; ++round) {
        for (const auto& [spawn_round, pearl] : scheduled_pearls)
            if (spawn_round == round)
                world.set_pearl(pearl);
        std::array<TurnAction, bodyguard_sim::MAX_DRAGONS> actions{};
        for (int dragon_id = 0; dragon_id < 6; ++dragon_id)
            actions[dragon_id] = script.action(round, dragon_id);
        world.run_round(actions);
        for (int dragon_id = 0; dragon_id < 6; ++dragon_id)
            std::cout << round << ' ' << dragon_id << ' ' << world.alive(dragon_id) << ' '
                      << world.head(dragon_id) << ' ' << world.tail(dragon_id) << ' '
                      << world.length(dragon_id) << '\n';
    }
    return 0;
}

} // namespace

void run_filter(std::string_view filter) {
    if (filter == "all" || filter == "movement") {
        test_wraps();
        test_kelp_and_body_collisions();
        test_pearl_growth_and_paid_step();
        test_portal_traversal();
    }
    if (filter == "all" || filter == "collisions")
        test_head_to_head();
    if (filter == "all" || filter == "splitting")
        test_splitting_and_death_drops();
    if (filter == "all" || filter == "sonar")
        test_sonar();
    if (filter == "all" || filter == "trace")
        test_deterministic_trace_batch();
}

int main(int argc, char** argv) {
    if (argc >= 2 && std::string_view(argv[1]) == "--state-trace")
        return state_trace(argc, argv);
    std::string_view filter = "all";
    if (argc == 3 && std::string_view(argv[1]) == "--filter")
        filter = argv[2];
    if (filter != "all" && filter != "movement" && filter != "collisions" && filter != "splitting" && filter != "sonar" && filter != "trace") {
        std::cerr << "unknown simulator filter\n";
        return 2;
    }
    run_filter(filter);
    std::cout << "bodyguard simulator tests passed\n";
    return 0;
}
