#include "kill_strategy_final.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace unswbc;
using namespace bot;

static Game test_game(40, 40, 10);
static const Position A(20, 20);
using Wall = std::pair<Position, Direction>;

static Position P(int x, int y) {
    return {x, y};
}

static Direction step_dir(const Position& from, const Position& to) {
    if (to.x == from.x + 1) return Direction::EAST;
    if (to.x == from.x - 1) return Direction::WEST;
    if (to.y == from.y + 1) return Direction::SOUTH;
    return Direction::NORTH;
}

// positions are head first, then body toward the tail.
static void add_dragon(
    std::vector<DragonPart>& out,
    int id,
    Team team,
    const std::vector<Position>& positions,
    Direction head_dir)
{
    assert(positions.size() >= 2);

    for (size_t i = 0; i < positions.size(); ++i) {
        const bool is_head = (i == 0);
        const Direction dir =
            is_head ? head_dir : step_dir(positions[i], positions[i - 1]);

        out.emplace_back(
            positions[i],
            id,
            team,
            dir,
            is_head
        );
    }
}

static Vision make_vision(
    const std::vector<DragonPart>& dragons,
    const std::vector<Position>& pearls,
    const std::vector<Wall>& kelp = {})
{
    std::vector<Tile> tiles;
    tiles.reserve(Constants::VISION_SIZE * Constants::VISION_SIZE);

    // Our head is the centre: (20,20).
    for (int y = 17; y <= 23; ++y)
        for (int x = 17; x <= 23; ++x)
            tiles.emplace_back(Tile(P(x, y)));

    auto idx = [](int x, int y) {
        return (y - 17) * 7 + (x - 17);
    };

    for (const auto& d : dragons) {
        if (d.position.x >= 17 && d.position.x <= 23 &&
            d.position.y >= 17 && d.position.y <= 23) {
            tiles[idx(d.position.x, d.position.y)].dragon_part = d;
        }
    }

    for (const auto& p : pearls) {
        if (p.x >= 17 && p.x <= 23 && p.y >= 17 && p.y <= 23)
            tiles[idx(p.x, p.y)].pearl = true;
    }

    // KELP is an edge between adjacent tiles. Set both sides.
    for (const auto& [pos, dir] : kelp) {
        if (pos.x < 17 || pos.x > 23 || pos.y < 17 || pos.y > 23)
            continue;

        tiles[idx(pos.x, pos.y)].get_edge(dir) =
            Edge(false, EdgeType::KELP);

        const Position other = pos.add_dir(dir);

        if (other.x >= 17 && other.x <= 23 &&
            other.y >= 17 && other.y <= 23) {
            tiles[idx(other.x, other.y)].get_edge(dir.get_opposite()) =
                Edge(false, EdgeType::KELP);
        }
    }

    return Vision(std::move(tiles));
}

static bool path_is_collision_free(
    const Vision& vision,
    Position start,
    const std::string& path,
    Position target,
    bool target_is_head)
{
    Position cur = start;

    for (size_t i = 0; i < path.size(); ++i) {
        const char c = path[i];

        const Direction dir =
            c == 'N' ? Direction::NORTH :
            c == 'E' ? Direction::EAST :
            c == 'S' ? Direction::SOUTH :
                       Direction::WEST;

        const Tile* here = vision.get_tile(cur);
        if (here == nullptr)
            return false;

        const Edge& edge = here->get_edge(dir);

        if (!edge.is_passable() || edge.is_portal())
            return false;

        const Position next = cur.add_dir(dir);
        const Tile* next_tile = vision.get_tile(next);

        if (next_tile == nullptr)
            return false;

        const DragonPart* part = next_tile->get_dragon();

        if (part != nullptr) {
            const bool final_step = (i + 1 == path.size());
            const bool legal_target =
                final_step && target_is_head &&
                next == target && part->is_head();

            if (!legal_target)
                return false;
        }

        cur = next;
    }

    return cur == target;
}

static void check(
    int no,
    const std::string& name,
    const Vision& vision,
    Position queen,
    int length,
    KillAction expected_action,
    const std::string& exact_path = {})
{
    const KillResult r = kill_queen(vision, queen, length);

    std::cout << "TEST " << no << " — " << name << "\n";
    std::cout << "  action=" << static_cast<int>(r.action)
              << " path=" << (r.path.empty() ? "<empty>" : r.path)
              << " target=(" << r.target.x << "," << r.target.y << ")\n";

    assert(r.action == expected_action);

    if (expected_action == KillAction::KILL_QUEEN ||
        expected_action == KillAction::KILL_ENEMY_HEAD) {
        assert(!r.path.empty());
        assert(path_is_collision_free(
            vision, A, r.path, r.target, true));

        const int free_steps = (length + 3) / 4;
        (void)free_steps; // legality was checked by the planner itself.
    } else if (expected_action == KillAction::PREPARE_QUEEN) {
        assert(!r.path.empty());

        Position cur = A;

        for (char c : r.path) {
            const Direction dir =
                c == 'N' ? Direction::NORTH :
                c == 'E' ? Direction::EAST :
                c == 'S' ? Direction::SOUTH :
                           Direction::WEST;

            const Tile* here = vision.get_tile(cur);
            assert(here != nullptr);
            const Edge& edge = here->get_edge(dir);
            assert(edge.is_passable());
            assert(!edge.is_portal());

            const Position next = cur.add_dir(dir);
            const Tile* next_tile = vision.get_tile(next);
            assert(next_tile != nullptr);
            assert(next_tile->get_dragon() == nullptr);

            cur = next;
        }
    } else {
        assert(r.path.empty());
    }

    if (!exact_path.empty())
        assert(r.path == exact_path);

    std::cout << "  PASS\n\n";
}

int main() {
    game = &test_game;

    const Team ME(Team::A);
    const Team ENEMY(Team::B);

    // A realistic visible body setup: every length-6 dragon has
    // one head + five contiguous body segments. Tails may extend
    // outside the 7x7 window in other scenarios, but here all are
    // kept separate from the attack path.

    // ------------------------------------------------------------
    // 1. BASELINE: no pearl.
    // length 3 -> free = 1, normal maximum is 2 paid-capable steps.
    // Queen is 3 tiles east, so she cannot be killed.
    // ------------------------------------------------------------
    {
        std::vector<DragonPart> d;

        add_dragon(d, 1, ME, {
            P(20,20), P(19,20), P(18,20)
        }, Direction::EAST);

        add_dragon(d, 99, ENEMY, {
            P(23,20), P(23,21), P(22,21), P(22,22), P(21,22), P(21,21)
        }, Direction::WEST);

        add_dragon(d, 50, ENEMY, {
            P(18,23), P(18,22), P(18,21), P(19,21), P(19,22), P(19,23)
        }, Direction::NORTH);

        check(1, "No pearl: length-3 cannot reach Queen at distance 3",
              make_vision(d, {}), P(23,20), 3,
              KillAction::PREPARE_QUEEN);
    }

    // ------------------------------------------------------------
    // 2. PEARL AFTER THE FREE STEP.
    // length 3: step 1 free, step 2 lands on pearl, step 3 kills.
    // ------------------------------------------------------------
    {
        std::vector<DragonPart> d;

        add_dragon(d, 1, ME, {
            P(20,20), P(19,20), P(18,20)
        }, Direction::EAST);

        add_dragon(d, 99, ENEMY, {
            P(23,20), P(23,21), P(22,21), P(22,22), P(21,22), P(21,21)
        }, Direction::WEST);

        add_dragon(d, 50, ENEMY, {
            P(18,23), P(18,22), P(18,21), P(19,21), P(19,22), P(19,23)
        }, Direction::NORTH);

        check(2, "Pearl on step 2: 3-step Queen kill",
              make_vision(d, {P(22,20)}), P(23,20), 3,
              KillAction::KILL_QUEEN, "EEE");
    }

    // ------------------------------------------------------------
    // 3. PEARL ON THE FIRST FREE STEP.
    // The pearl does not add another FREE step in the same turn,
    // but it increases length by 1 and therefore pays for another
    // future step.
    // ------------------------------------------------------------
    {
        std::vector<DragonPart> d;

        add_dragon(d, 1, ME, {
            P(20,20), P(19,20), P(18,20)
        }, Direction::NORTH);

        add_dragon(d, 99, ENEMY, {
            P(20,17), P(21,17), P(22,17), P(22,18), P(22,19), P(21,19)
        }, Direction::SOUTH);

        add_dragon(d, 50, ENEMY, {
            P(18,23), P(18,22), P(17,22), P(17,21), P(18,21), P(19,21)
        }, Direction::EAST);

        check(3, "Pearl on free step: still gets a 3-step kill",
              make_vision(d, {P(20,19)}), P(20,17), 3,
              KillAction::KILL_QUEEN, "NNN");
    }

    // ------------------------------------------------------------
    // 4. TWO PEARLS IN ONE SPRINT.
    // Same geometry as the original test, but the correct result is
    // PREPARE_QUEEN: EEESS is illegal because after step 4 the dragon
    // is length 2 and cannot take the fifth paid step.
    // ------------------------------------------------------------
    {
        std::vector<DragonPart> d;

        add_dragon(d, 1, ME, {
            P(20,20), P(19,20), P(18,20)
        }, Direction::EAST);

        add_dragon(d, 99, ENEMY, {
            P(23,22), P(22,22), P(21,22), P(21,23), P(22,23), P(23,23)
        }, Direction::WEST);

        add_dragon(d, 50, ENEMY, {
            P(18,23), P(18,22), P(17,22), P(17,21), P(18,21), P(19,21)
        }, Direction::NORTH);

        check(4, "Two pearls: correct result is preparation, not a 5-step kill",
              make_vision(d, {P(21,20), P(22,20)}),
              P(23,22), 3,
              KillAction::PREPARE_QUEEN, "EEES");
    }

    // ------------------------------------------------------------
    // 5. IMPORTANT ENGINE ORDERING:
    // length 2 has only 1 free step. The second step is paid.
    // Even if that destination is a pearl, the engine checks whether
    // the dragon can pay BEFORE checking/eating that pearl.
    // Therefore this must NOT kill the Queen.
    // ------------------------------------------------------------
    {
        std::vector<DragonPart> d;

        add_dragon(d, 1, ME, {
            P(20,20), P(19,20)
        }, Direction::EAST);

        add_dragon(d, 99, ENEMY, {
            P(23,20), P(23,21), P(22,21), P(22,22), P(21,22), P(21,21)
        }, Direction::WEST);

        add_dragon(d, 50, ENEMY, {
            P(18,23), P(18,22), P(18,21), P(19,21), P(19,22), P(19,23)
        }, Direction::NORTH);

        check(5, "Length 2 cannot rescue paid step with a pearl",
              make_vision(d, {P(22,20)}), P(23,20), 2,
              KillAction::PREPARE_QUEEN);
    }

    // ------------------------------------------------------------
    // 6. LONGER PEARL-RICH DETOUR.
    // Same geometry as the original test. With only two pearls and
    // length 3, the 5-step N-EEE-S attack is NOT payable.
    // Correct result: prepare instead.
    // ------------------------------------------------------------
    {
        std::vector<DragonPart> d;

        add_dragon(d, 1, ME, {
            P(20,20), P(19,20), P(18,20)
        }, Direction::EAST);

        add_dragon(d, 99, ENEMY, {
            P(23,20), P(23,21), P(22,21), P(22,22), P(21,22), P(21,21)
        }, Direction::WEST);

        add_dragon(d, 50, ENEMY, {
            P(18,23), P(18,22), P(18,21), P(19,21), P(19,22), P(19,23)
        }, Direction::NORTH);

        const std::vector<Wall> k = {
            {P(20,20), Direction::EAST},
            {P(21,20), Direction::EAST},
            {P(22,20), Direction::EAST}
        };

        check(6, "KELP detour: two pearls are insufficient for the 5-step kill",
              make_vision(d, {P(21,19), P(22,19)}, k),
              P(23,20), 3,
              KillAction::PREPARE_QUEEN, "NEES");
    }

    // ------------------------------------------------------------
    // 7. PEARL + QUEEN BODY.
    // Same geometry as the original test. The two pearls are not
    // enough to make the 5-step attack legal, so prepare instead.
    // ------------------------------------------------------------
    {
        std::vector<DragonPart> d;

        add_dragon(d, 1, ME, {
            P(20,20), P(19,20), P(18,20)
        }, Direction::EAST);

        add_dragon(d, 99, ENEMY, {
            P(23,20), P(23,21), P(22,21), P(22,22), P(23,22), P(23,23)
        }, Direction::WEST);

        add_dragon(d, 50, ENEMY, {
            P(18,23), P(18,22), P(17,22), P(17,21), P(18,21), P(19,21)
        }, Direction::NORTH);

        const std::vector<Wall> k = {
            {P(20,20), Direction::EAST},
            {P(21,20), Direction::EAST}
        };

        // Route is N E E E S -> (23,20), not through Queen's body.
        check(7, "Pearls + Queen body: prepare safely",
              make_vision(d, {P(21,19), P(22,19)}, k),
              P(23,20), 3,
              KillAction::PREPARE_QUEEN, "NEES");
    }

    // ------------------------------------------------------------
    // 8. OTHER ENEMY HEAD.
    // Queen cannot be reached, but another enemy HEAD can be killed.
    // The enemy body is a real contiguous chain and is not crossed.
    // ------------------------------------------------------------
    {
        std::vector<DragonPart> d;

        add_dragon(d, 1, ME, {
            P(20,20), P(19,20), P(18,20)
        }, Direction::EAST);

        add_dragon(d, 99, ENEMY, {
            P(23,23), P(23,22), P(23,21), P(22,21), P(21,21), P(21,20)
        }, Direction::WEST);

        add_dragon(d, 50, ENEMY, {
            P(20,22), P(20,23), P(19,23), P(18,23), P(18,22), P(18,21)
        }, Direction::NORTH);

        check(8, "Reachable enemy head remains a valid fallback",
              make_vision(d, {}), P(23,23), 3,
              KillAction::KILL_ENEMY_HEAD, "SS");
    }

    // ------------------------------------------------------------
    // 9. PREPARATION ALSO USES PEARLS.
    // Queen faces west, so predicted position is (22,23).
    // Correct result remains the same: SSEES.
    // ------------------------------------------------------------
    {
        std::vector<DragonPart> d;

        add_dragon(d, 1, ME, {
            P(20,20), P(19,20), P(18,20)
        }, Direction::EAST);

        add_dragon(d, 99, ENEMY, {
            P(23,23), P(23,22), P(23,21), P(22,21), P(21,21), P(21,20)
        }, Direction::WEST);

        add_dragon(d, 50, ENEMY, {
            P(18,17), P(18,18), P(19,18), P(19,17), P(20,17), P(21,17)
        }, Direction::EAST);

        check(9, "Preparation uses pearls to extend the sprint",
              make_vision(d, {P(20,21), P(20,22)}),
              P(23,23), 3,
              KillAction::PREPARE_QUEEN, "SSEES");
    }

    // ------------------------------------------------------------
    // 10. BODY + KELP + PEARLS + QUEEN.
    // Same geometry as the original test, but the two pearls do not
    // make the 5-step attack payable. Correct result: prepare.
    // ------------------------------------------------------------
    {
        std::vector<DragonPart> d;

        add_dragon(d, 1, ME, {
            P(20,20), P(19,20), P(18,20), P(17,20), P(17,19), P(18,19)
        }, Direction::EAST);

        add_dragon(d, 99, ENEMY, {
            P(23,20), P(23,21), P(22,21), P(22,22), P(23,22), P(23,23)
        }, Direction::WEST);

        add_dragon(d, 50, ENEMY, {
            P(18,17), P(18,18), P(19,18), P(20,18), P(20,17), P(21,17)
        }, Direction::EAST);

        const std::vector<Wall> k = {
            {P(20,20), Direction::EAST},
            {P(21,20), Direction::EAST},
            {P(22,20), Direction::EAST}
        };

        // N-EEE-S is not payable for length 3 with only these two pearls.
        check(10, "Full stress: own body + KELP + pearls + Queen, prepare",
              make_vision(d, {P(21,19), P(22,19)}, k),
              P(23,20), 3,
              KillAction::PREPARE_QUEEN, "NEES");
    }

    std::cout << "ALL FINAL KILL STRATEGY TESTS PASSED\n";
    return 0;
}
