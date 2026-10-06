#pragma once

#include "../../helper.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace bot {

enum class KillAction {
    NONE,
    KILL_QUEEN,
    KILL_ENEMY_HEAD,
    PREPARE_QUEEN
};

struct KillResult {
    KillAction action = KillAction::NONE;
    std::string path;       // e.g. "NWNWEE"
    unswbc::Position target{};
};

/*
    Local Queen-kill / preparation planner.

    Inputs:
      - current 7x7 Vision
      - visible enemy Queen head position
      - our ACTUAL dragon length

    Priority:
      1. Kill Queen this turn, if legally possible.
      2. Otherwise kill another enemy dragon HEAD this turn.
      3. Otherwise prepare for the Queen.

    Collision rules:
      - own body is blocked
      - Queen body is blocked
      - other enemy bodies are blocked
      - another enemy HEAD is blocked unless that exact head is the target
      - KELP edges are blocked
      - PORTAL edges are not used by this local planner

    Sprint / pearl rules:
      - first ceil(L / 4) steps are free, with L fixed at move start
      - after that, a step must be payable BEFORE the destination is checked
      - a normal paid step removes one segment
      - stepping onto a pearl grows the dragon by one and the tail does NOT
        advance, so that step removes no segment
      - therefore a pearl after the free portion can create more than just
        "one extra step": it both costs no segment and gives +1 length
      - a pearl cannot rescue an otherwise unpayable step, because the engine
        checks whether the dragon can pay before it checks/eats the destination
      - pearl tiles are one-time resources; the search remembers which visible
        pearls it has already eaten
      - a PAID + PEARL step has net length change 0:
            +1 for eating the pearl, -1 for paying the paid step

    The free-step count is fixed from the length at the START of the move.
    The current length is then updated after every successful step and is
    used to decide whether the next paid step can be afforded.
*/

inline KillResult kill_queen(
    const unswbc::Vision& vision,
    unswbc::Position queen_head,
    int length)
{
    KillResult result;

    const auto& tiles = vision.get_tiles();

    constexpr int V = unswbc::Constants::VISION_SIZE;
    constexpr int N = V * V;

    if (static_cast<int>(tiles.size()) != N)
        return result;

    if (length < unswbc::Constants::MIN_SIZE)
        return result;

    // ---------------------------------------------------------
    // The 25th tile is the centre of the 7x7 vision = our head.
    // ---------------------------------------------------------
    const auto* my_head_part = tiles[24].get_dragon();
    if (my_head_part == nullptr || !my_head_part->is_head())
        return result;

    const int my_id = my_head_part->get_id();
    const unswbc::Team my_team = my_head_part->get_team();
    const unswbc::Position my_head = tiles[24].get_position();

    // ---------------------------------------------------------
    // Verify the supplied Queen coordinate.
    // ---------------------------------------------------------
    const unswbc::Tile* queen_tile = vision.get_tile(queen_head);
    if (queen_tile == nullptr)
        return result;

    const auto* queen_part = queen_tile->get_dragon();
    if (queen_part == nullptr || !queen_part->is_head())
        return result;

    const unswbc::Team enemy_team = queen_part->get_team();

    if (enemy_team == my_team)
        return result;

    const auto dirs = unswbc::Direction::get_direction_list();

    // ---------------------------------------------------------
    // Position -> index in the visible 49-tile graph.
    // ---------------------------------------------------------
    std::unordered_map<unswbc::Position, int, unswbc::PositionHash> index;
    index.reserve(tiles.size() * 2);

    for (int i = 0; i < static_cast<int>(tiles.size()); ++i)
        index[tiles[i].get_position()] = i;

    const auto start_it = index.find(my_head);
    const auto queen_it = index.find(queen_head);

    if (start_it == index.end() || queen_it == index.end())
        return result;

    const int start = start_it->second;

    // ---------------------------------------------------------
    // Pearl bit mask. 49 visible tiles fit in uint64_t.
    // ---------------------------------------------------------
    std::uint64_t pearl_bits = 0;

    for (int i = 0; i < N; ++i) {
        if (tiles[i].has_pearl())
            pearl_bits |= (std::uint64_t{1} << i);
    }

    const int visible_pearls = static_cast<int>(
        __builtin_popcountll(pearl_bits)
    );

    const int free_steps = (length + 3) / 4;

    // ---------------------------------------------------------
    // Search state for a legally executable sprint.
    //
    // "length" is the CURRENT length during the sprint.
    // "pearl_mask" records pearls already eaten in this sprint.
    // ---------------------------------------------------------
    struct State {
        int tile = -1;
        int length = 0;
        int steps = 0;
        std::uint64_t pearl_mask = 0;
        int parent = -1;
        int parent_dir = -1;
    };

    struct Key {
        int tile = -1;
        int length = 0;
        std::uint64_t pearl_mask = 0;

        bool operator==(const Key& other) const {
            return tile == other.tile &&
                   length == other.length &&
                   pearl_mask == other.pearl_mask;
        }
    };

    struct KeyHash {
        std::size_t operator()(const Key& k) const {
            std::size_t h = std::hash<int>{}(k.tile);
            h ^= std::hash<int>{}(k.length) +
                 static_cast<std::size_t>(0x9e3779b9) +
                 (h << 6) + (h >> 2);
            h ^= std::hash<std::uint64_t>{}(k.pearl_mask) +
                 static_cast<std::size_t>(0x9e3779b9) +
                 (h << 6) + (h >> 2);
            return h;
        }
    };

    /*
        Find the shortest LEGAL sprint to a target.

        The search is BFS by number of steps, but state also contains the
        current length and the set of pearls already eaten. Thus a longer
        pearl-rich route can beat the old fixed sprint limit.
    */
    auto legal_path_to =
        [&](unswbc::Position target, bool target_must_be_head)
        -> std::string
    {
        const auto target_it = index.find(target);
        if (target_it == index.end())
            return {};

        const int goal = target_it->second;

        std::vector<State> states;
        states.reserve(4096);

        std::queue<int> q;

        std::unordered_map<Key, int, KeyHash> best;
        best.reserve(8192);

        State first;
        first.tile = start;
        first.length = length;
        first.steps = 0;
        first.pearl_mask = 0;

        states.push_back(first);

        best[{start, length, 0}] = 0;
        q.push(0);

        /*
            The search only needs to explore a finite useful horizon.
            Each visible pearl can increase the available length, so allow
            substantially more than the old fixed sprint limit.
        */
        /*
            Without pearls, the maximum number of successful steps is:
                free_steps + length - 2

            Every eaten pearl adds one extra successful step relative to
            the no-pearl case:
              - free + pearl: +1 length
              - paid + pearl: +1 growth and -1 payment => net 0,
                which saves one segment compared with paid + no pearl.

            Thus visible pearls give an upper bound on additional sprint
            steps. This is only a search bound; the state simulation below
            decides whether each individual step is actually legal.
        */
        const int max_useful_steps =
            free_steps + length - 2 + visible_pearls;

        while (!q.empty()) {
            const int state_id = q.front();
            q.pop();

            const State cur = states[state_id];

            if (cur.tile == goal)
                break;

            if (cur.steps >= max_useful_steps)
                continue;

            const auto& tile = tiles[cur.tile];
            const auto pos = tile.get_position();

            for (int d = 0; d < 4; ++d) {
                const auto dir = dirs[d];
                const auto& edge = tile.get_edge(dir);

                // KELP kills on the edge.
                if (!edge.is_passable())
                    continue;

                // A portal leaves the local 7x7 graph, so do not plan through it.
                if (edge.is_portal())
                    continue;

                const auto next_pos = pos.add_dir(dir);
                const auto next_it = index.find(next_pos);

                if (next_it == index.end())
                    continue;

                const int next = next_it->second;
                const bool final_step = (next == goal);

                const auto* dragon = tiles[next].get_dragon();

                if (dragon != nullptr) {
                    const bool valid_target =
                        final_step &&
                        target_must_be_head &&
                        dragon->is_head() &&
                        next_pos == target;

                    // Every occupied tile is blocked except the exact
                    // enemy head we intentionally want to kill.
                    if (!valid_target)
                        continue;
                }

                const int step_number = cur.steps + 1;
                const bool paid_step = step_number > free_steps;

                /*
                    Payment is checked BEFORE the destination is checked.
                    Therefore if current length is 2 and this is a paid step,
                    the move is illegal even when the destination is a pearl.
                */
                if (paid_step && cur.length <= unswbc::Constants::MIN_SIZE)
                    continue;

                std::uint64_t next_mask = cur.pearl_mask;
                int next_length = cur.length;

                const std::uint64_t bit = std::uint64_t{1} << next;

                const bool pearl_here =
                    (tiles[next].has_pearl() &&
                     (next_mask & bit) == 0);

                if (pearl_here)
                    next_mask |= bit;

                // A pearl adds one segment because the tail does not
                // advance. A paid step removes one segment. Therefore
                // paid+pearl has net change 0.
                if (pearl_here)
                    ++next_length;

                if (paid_step)
                    --next_length;

                if (next_length < unswbc::Constants::MIN_SIZE)
                    continue;

                State nxt;
                nxt.tile = next;
                nxt.length = next_length;
                nxt.steps = step_number;
                nxt.pearl_mask = next_mask;
                nxt.parent = state_id;
                nxt.parent_dir = d;

                Key key{next, next_length, next_mask};

                /*
                    Reaching the same tile with the same current length and
                    same consumed-pearl set in fewer steps dominates the later
                    state, because it leaves at least as much sprint freedom.
                */
                if (best.contains(key))
                    continue;

                const int new_id = static_cast<int>(states.size());
                states.push_back(nxt);
                best[key] = new_id;
                q.push(new_id);
            }
        }

        int goal_state = -1;

        // BFS guarantees the first goal discovered is shortest by steps,
        // but the queue may stop with the goal not yet expanded.
        for (int i = 0; i < static_cast<int>(states.size()); ++i) {
            if (states[i].tile == goal) {
                if (goal_state == -1 ||
                    states[i].steps < states[goal_state].steps) {
                    goal_state = i;
                }
            }
        }

        if (goal_state == -1)
            return {};

        std::string path;

        for (int cur = goal_state; cur != 0; cur = states[cur].parent) {
            if (states[cur].parent == -1 ||
                states[cur].parent_dir == -1)
                return {};

            path.push_back(dirs[states[cur].parent_dir].value);
        }

        std::reverse(path.begin(), path.end());
        return path;
    };

    // ---------------------------------------------------------
    // Geometric shortest path, used only for Queen preparation.
    // It does not cross occupied tiles or portals.
    // ---------------------------------------------------------
    auto shortest_geometric_path =
        [&](unswbc::Position target, bool target_can_be_occupied)
        -> std::string
    {
        const auto target_it = index.find(target);
        if (target_it == index.end())
            return {};

        const int goal = target_it->second;
        const int n = static_cast<int>(tiles.size());

        std::vector<bool> visited(n, false);
        std::vector<int> parent(n, -1);
        std::vector<int> parent_dir(n, -1);
        std::queue<int> q;

        visited[start] = true;
        q.push(start);

        while (!q.empty()) {
            const int cur = q.front();
            q.pop();

            if (cur == goal)
                break;

            const auto& tile = tiles[cur];
            const auto pos = tile.get_position();

            for (int d = 0; d < 4; ++d) {
                const auto dir = dirs[d];
                const auto& edge = tile.get_edge(dir);

                if (!edge.is_passable() || edge.is_portal())
                    continue;

                const auto next_pos = pos.add_dir(dir);
                const auto next_it = index.find(next_pos);

                if (next_it == index.end())
                    continue;

                const int next = next_it->second;

                if (visited[next])
                    continue;

                const auto* dragon = tiles[next].get_dragon();

                if (dragon != nullptr) {
                    const bool legal_target =
                        next == goal && target_can_be_occupied;

                    if (!legal_target)
                        continue;
                }

                visited[next] = true;
                parent[next] = cur;
                parent_dir[next] = d;
                q.push(next);
            }
        }

        if (!visited[goal])
            return {};

        std::string path;

        for (int cur = goal; cur != start; cur = parent[cur]) {
            if (parent[cur] == -1 || parent_dir[cur] == -1)
                return {};

            path.push_back(dirs[parent_dir[cur]].value);
        }

        std::reverse(path.begin(), path.end());
        return path;
    };

    // ---------------------------------------------------------
    // Simulate how many steps of a preparation path are legal this turn.
    // Pearls on the path are consumed once.
    // ---------------------------------------------------------
    auto legal_prefix =
        [&](const std::string& path)
        -> int
    {
        int current_length = length;
        int step_number = 0;
        std::uint64_t eaten = 0;
        unswbc::Position cur = my_head;

        for (char c : path) {
            const auto dir =
                c == 'N' ? unswbc::Direction::NORTH :
                c == 'E' ? unswbc::Direction::EAST :
                c == 'S' ? unswbc::Direction::SOUTH :
                           unswbc::Direction::WEST;

            const auto* here = vision.get_tile(cur);
            if (here == nullptr)
                break;

            const auto& edge = here->get_edge(dir);
            if (!edge.is_passable() || edge.is_portal())
                break;

            const auto next_pos = cur.add_dir(dir);
            const auto* next_tile = vision.get_tile(next_pos);
            if (next_tile == nullptr)
                break;

            if (const auto* dragon = next_tile->get_dragon();
                dragon != nullptr) {
                // Preparation never enters an occupied tile.
                break;
            }

            ++step_number;
            const bool paid_step = step_number > free_steps;

            if (paid_step &&
                current_length <= unswbc::Constants::MIN_SIZE)
                break;

            const auto it = index.find(next_pos);
            if (it == index.end())
                break;

            const int next_index = it->second;
            const std::uint64_t bit = std::uint64_t{1} << next_index;

            const bool pearl_here =
                next_tile->has_pearl() &&
                (eaten & bit) == 0;

            if (pearl_here) {
                eaten |= bit;
                ++current_length;
            }

            // Paid + pearl: +1 growth and -1 paid-step cost => net 0.
            if (paid_step)
                --current_length;

            if (current_length < unswbc::Constants::MIN_SIZE)
                break;

            cur = next_pos;
        }

        return step_number;
    };

    // ---------------------------------------------------------
    // 1. KILL QUEEN THIS TURN
    // ---------------------------------------------------------
    const std::string queen_path =
        legal_path_to(queen_head, true);

    if (!queen_path.empty()) {
        result.action = KillAction::KILL_QUEEN;
        result.path = queen_path;
        result.target = queen_head;
        return result;
    }

    // ---------------------------------------------------------
    // 2. KILL ANOTHER ENEMY HEAD
    //
    // Only a HEAD can be a kill target.
    // ---------------------------------------------------------
    std::string best_enemy_path;
    unswbc::Position best_enemy{};

    for (const auto& tile : tiles) {
        const auto* part = tile.get_dragon();

        if (part == nullptr ||
            !part->is_head() ||
            !(part->get_team() == enemy_team) ||
            part->get_id() == my_id) {
            continue;
        }

        const auto target = tile.get_position();

        if (target == queen_head)
            continue;

        const std::string path =
            legal_path_to(target, true);

        if (path.empty())
            continue;

        if (best_enemy_path.empty() ||
            path.size() < best_enemy_path.size()) {
            best_enemy_path = path;
            best_enemy = target;
        }
    }

    if (!best_enemy_path.empty()) {
        result.action = KillAction::KILL_ENEMY_HEAD;
        result.path = best_enemy_path;
        result.target = best_enemy;
        return result;
    }

    // ---------------------------------------------------------
    // 3. PREPARE FOR THE QUEEN
    //
    // Predict her next position from her current facing.
    // We use pearls when they occur on the chosen preparation path.
    // ---------------------------------------------------------
    unswbc::Position predicted = queen_head;

    const auto predicted_pos =
        queen_head.add_dir(queen_part->get_dir());

    const auto* predicted_tile =
        vision.get_tile(predicted_pos);

    if (predicted_tile != nullptr &&
        predicted_tile->get_dragon() == nullptr) {
        predicted = predicted_pos;
    }

    std::string prep_path =
        shortest_geometric_path(predicted, false);

    if (prep_path.empty()) {
        // Queen's current tile is occupied, so aim for the best nearby
        // empty neighbour instead.
        std::string best_near_queen;

        for (const auto dir : dirs) {
            const auto neighbour = queen_head.add_dir(dir);
            const auto* t = vision.get_tile(neighbour);

            if (t == nullptr || t->get_dragon() != nullptr)
                continue;

            const std::string p =
                shortest_geometric_path(neighbour, false);

            if (!p.empty() &&
                (best_near_queen.empty() ||
                 p.size() < best_near_queen.size())) {
                best_near_queen = p;
            }
        }

        prep_path = best_near_queen;
    }

    if (!prep_path.empty()) {
        const int steps = legal_prefix(prep_path);

        if (steps > 0) {
            result.action = KillAction::PREPARE_QUEEN;
            result.path = prep_path.substr(0, steps);
            result.target = predicted;
            return result;
        }
    }

    return result;
}

} // namespace bot
