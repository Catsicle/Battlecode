#include "scout_nav.hpp"
#include "transition.hpp"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <limits>
#include <queue>
#include <vector>

namespace bot::nav::scout {
namespace {

constexpr int STEP_COST        = 10;
constexpr int VISITED_COST     = 25;
constexpr int BANNED_COST      = 150;
constexpr int PORTAL_BONUS     = 120;
constexpr int COMMIT_BONUS     = 100000;
constexpr int BAN_RADIUS       = 2;
constexpr int MAX_CROSS_FAILS  = 3;
constexpr int MAX_BLOCKED_TURNS = 40;   // turns with no path back before abandoning a frame

enum PState : std::uint8_t { Unvisited = 0, Active = 1, Done = 2 };

struct Frame {
    int portal;
    int entry_cell;
    int entry_dir;
};

struct State {
    std::vector<std::uint8_t> pstate;
    std::vector<std::uint8_t> pfail;
    std::vector<std::uint8_t> banned;
    std::vector<Frame> stack;
    bool pending = false;
    int pend_cell = -1, pend_dir = -1, pend_portal = -1;
    int planned_cell = -1, planned_dir = -1;
    int commit_cell = -1, commit_dir = -1;
    int blocked_turns = 0;
};
State S;

struct Node {
    int cost;
    int cell;
    bool operator>(const Node& o) const { return cost > o.cost; }
};

void ensure(const Memory& m) {
    if (static_cast<int>(S.banned.size()) != m.cells)
        S.banned.assign(m.cells, 0);
    if (S.pstate.size() < m.portal_ends.size())
        S.pstate.resize(m.portal_ends.size(), Unvisited);
    if (S.pfail.size() < m.portal_ends.size())
        S.pfail.resize(m.portal_ends.size(), 0);
}

PState get_state(int pid) {
    return pid >= 0 && pid < static_cast<int>(S.pstate.size()) ? static_cast<PState>(S.pstate[pid])
                                                                : Unvisited;
}

void set_state(int pid, PState s) {
    if (pid >= static_cast<int>(S.pstate.size()))
        S.pstate.resize(pid + 1, Unvisited);
    S.pstate[pid] = s;
}

void clear_commit() { S.commit_cell = S.commit_dir = -1; }

// (c, d) is one of the two recorded doorways of the pair, not an alias side of the same edge.
bool is_registered_end(const Memory& m, int pid, int c, int d) {
    if (pid < 0 || pid >= static_cast<int>(m.portal_ends.size()))
        return false;
    const PortalEnds& e = m.portal_ends[pid];
    for (int k = 0; k < e.count; ++k)
        if (e.tile[k] == c && e.direction[k] == d)
            return true;
    return false;
}

void ban_around(const Memory& m, int cell, int r) {
    if (cell < 0)
        return;
    Vec2 p = m.position(cell);
    auto wrap = [](int v, int size) { return (v % size + size) % size; };
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx) {
            if (std::abs(dx) + std::abs(dy) > r)
                continue;
            int idx = m.index(Vec2{wrap(p.x + dx, m.width), wrap(p.y + dy, m.height)});
            if (idx >= 0)
                S.banned[idx] = 1;
        }
}

void ban_portal(const Memory& m, int pid) {
    if (pid < 0 || pid >= static_cast<int>(m.portal_ends.size()))
        return;
    const auto& ends = m.portal_ends[pid];
    for (int k = 0; k < ends.count; ++k) {
        ban_around(m, ends.tile[k], BAN_RADIUS);
        Vec2 emerge = add(m.position(ends.tile[k]), directions()[ends.direction[k]], m.width, m.height);
        ban_around(m, m.index(emerge), BAN_RADIUS);
    }
}

void retire_top_frame(const Memory& m) {
    int pid = S.stack.back().portal;
    S.stack.pop_back();
    set_state(pid, Done);
    ban_portal(m, pid);
    S.blocked_turns = 0;
}

bool real_frontier(const Memory& m, int c) {
    Vec2 p = m.position(c);
    for (Dir dir : directions()) {
        int d = dir_index(dir);
        if ((m.kelp[c] & (1U << d)) != 0 || m.portal_id[c][d] >= 0)
            continue;
        if (!m.known(m.index(add(p, dir, m.width, m.height))))
            return true;
    }
    return false;
}

// Walk over ordinary edges only. If `from` reaches `to`, the land behind the portal is not an island.
bool land_connected(const Grid& grid, int from, int to) {
    const Memory& m = grid.memory;
    if (from < 0 || to < 0)
        return false;
    if (from == to)
        return true;
    std::vector<std::uint8_t> seen(m.cells, 0);
    std::vector<int> q;
    q.push_back(from);
    seen[from] = 1;
    for (std::size_t head = 0; head < q.size(); ++head) {
        int c = q[head];
        for (Dir dir : directions()) {
            int d = dir_index(dir);
            if (m.portal_id[c][d] >= 0 || !grid.passable_edge(c, d))
                continue;
            Step st = resolve_step(m, c, d);
            if (st.kind != StepKind::Normal || st.dest < 0)
                continue;
            int next = st.dest;
            if (seen[next] || !grid.known(next))
                continue;
            if (next == to)
                return true;
            seen[next] = 1;
            q.push_back(next);
        }
    }
    return false;
}

Path search(const Grid& grid, int start, std::uint8_t forbid) {
    const Memory& m = grid.memory;
    const int n = m.cells;
    const int INF = std::numeric_limits<int>::max();

    std::vector<int> dist(n, INF), parent(n, -1);
    std::vector<std::int8_t> pdir(n, -1);
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> q;
    dist[start] = 0;
    q.push({0, start});

    const bool in_frame = !S.stack.empty();
    const int top_portal = in_frame ? S.stack.back().portal : -1;
    const int top_entry  = in_frame ? S.stack.back().entry_cell : -1;

    int goal = -1, goal_dir = -1, goal_score = INF;
    int ret = -1, ret_dir = -1, ret_cost = INF;
    int mop = -1, mop_cost = INF;
    bool commit_found = false;

    while (!q.empty()) {
        Node cur = q.top();
        q.pop();
        if (cur.cost != dist[cur.cell])
            continue;
        if (commit_found)
            break;   // the committed portal always wins, nothing else matters
        if (goal >= 0 && S.commit_cell < 0 && cur.cost - PORTAL_BONUS >= goal_score)
            break;
        const int c = cur.cell;
        const bool is_start = (c == start);

        for (int d = 0; d < 4; ++d) {
            int pid = m.portal_id[c][d];
            if (pid < 0)
                continue;
            if (is_start && (forbid & (1U << d)) != 0)
                continue;
            Step ps = resolve_step(m, c, d);
            if (ps.kind == StepKind::Kelp)
                continue;   // far or near edge blocked: never a usable crossing
            PState st = get_state(pid);
            if (st == Unvisited && !S.banned[c] && is_registered_end(m, pid, c, d)) {
                const bool committed = (c == S.commit_cell && d == S.commit_dir);
                if (committed)
                    commit_found = true;
                int score = cur.cost - PORTAL_BONUS - (committed ? COMMIT_BONUS : 0);
                if (score < goal_score) { goal = c; goal_dir = d; goal_score = score; }
            } else if (st == Active && pid == top_portal && c != top_entry) {
                if (cur.cost < ret_cost) { ret = c; ret_dir = d; ret_cost = cur.cost; }
            }
        }
        if (!is_start && !S.banned[c]) {
            if (real_frontier(m, c)) {
                if (cur.cost < goal_score) { goal = c; goal_dir = -1; goal_score = cur.cost; }
            }
            if (m.last_visit_round[c] < 0 && cur.cost < mop_cost) {
                mop = c;
                mop_cost = cur.cost;
            }
        }

        for (Dir dir : directions()) {
            int d = dir_index(dir);
            if (m.portal_id[c][d] >= 0)
                continue;
            if (is_start && (forbid & (1U << d)) != 0)
                continue;
            if (!grid.passable_edge(c, d))
                continue;
            Step st = resolve_step(m, c, d);
            if (st.kind != StepKind::Normal || st.dest < 0)
                continue;
            int next = st.dest;
            if (!grid.known(next) || m.occ_fresh(next, grid.round))
                continue;
            int cost = cur.cost + STEP_COST
                     + (m.last_visit_round[next] >= 0 ? VISITED_COST : 0)
                     + (S.banned[next] ? BANNED_COST : 0);
            if (cost >= dist[next])
                continue;
            dist[next] = cost;
            parent[next] = c;
            pdir[next] = static_cast<std::int8_t>(d);
            q.push({cost, next});
        }
    }

    // A commitment we could not reach this turn is dropped, so we don't scan the map forever.
    if (S.commit_cell >= 0 && !commit_found)
        clear_commit();

    auto build = [&](int target, int portal_dir) {
        Path path;
        std::vector<Dir> rev;
        for (int c = target; c != start; c = parent[c])
            rev.push_back(directions()[pdir[c]]);
        path.steps.assign(rev.rbegin(), rev.rend());
        if (portal_dir >= 0) {
            path.steps.push_back(directions()[portal_dir]);
            S.planned_cell = target;
            S.planned_dir = portal_dir;
            if (get_state(m.portal_id[target][portal_dir]) == Unvisited) {
                S.commit_cell = target;
                S.commit_dir = portal_dir;
            }
        }
        path.length = static_cast<int>(path.steps.size());
        path.cost = dist[target];
        path.found = path.length > 0;
        return path;
    };

    if (goal >= 0) return build(goal, goal_dir);
    if (ret >= 0)  return build(ret, ret_dir);
    if (mop >= 0)  return build(mop, -1);
    return {};
}

} // namespace

void begin_turn(const Memory& m, Vec2 head) {
    ensure(m);
    if (!S.pending)
        return;
    S.pending = false;
    const int pid = S.pend_portal;

    if (m.index(head) == S.pend_cell) {
        if (pid >= 0 && pid < static_cast<int>(S.pfail.size()) && ++S.pfail[pid] >= MAX_CROSS_FAILS) {
            S.pfail[pid] = 0;
            clear_commit();
            if (get_state(pid) == Active && !S.stack.empty() && S.stack.back().portal == pid) {
                retire_top_frame(m);
            } else {
                set_state(pid, Done);
                ban_portal(m, pid);
            }
        }
        return;
    }

    if (pid >= 0 && pid < static_cast<int>(S.pfail.size()))
        S.pfail[pid] = 0;
    clear_commit();

    if (get_state(pid) == Unvisited) {
        set_state(pid, Active);
        S.stack.push_back({pid, S.pend_cell, S.pend_dir});
    } else if (get_state(pid) == Active && !S.stack.empty() && S.stack.back().portal == pid) {
        retire_top_frame(m);
    }
}

Path plan(const Grid& grid, Vec2 start_pos, std::uint8_t forbid) {
    const Memory& m = grid.memory;
    ensure(m);
    S.planned_cell = S.planned_dir = -1;
    int start = m.index(start_pos);
    if (start < 0 || !grid.known(start))
        return {};

    if (S.commit_cell >= 0) {
        int pid = (S.commit_dir >= 0) ? m.portal_id[S.commit_cell][S.commit_dir] : -1;
        if (pid < 0 || get_state(pid) != Unvisited || S.banned[S.commit_cell])
            clear_commit();
    }

    // Not an island: the land behind the portal reaches back to where we entered.
    while (!S.stack.empty() && land_connected(grid, start, S.stack.back().entry_cell))
        retire_top_frame(m);

    for (int guard = 0; guard < 16; ++guard) {
        Path path = search(grid, start, forbid);
        if (path.found) {
            S.blocked_turns = 0;
            return path;
        }
        if (S.stack.empty() || forbid != 0)
            return {};
        // No path now. If a way back exists it is probably just temporarily blocked
        // (enemy on the return tile), so wait instead of abandoning the island.
        const int pid = S.stack.back().portal;
        const bool has_way_back = pid >= 0 && pid < static_cast<int>(m.portal_ends.size()) &&
                                  m.portal_ends[pid].count == 2;
        if (has_way_back && ++S.blocked_turns < MAX_BLOCKED_TURNS)
            return {};
        retire_top_frame(m);
    }
    return {};
}

bool is_planned_crossing(int cell, int dir) {
    return cell >= 0 && cell == S.planned_cell && dir == S.planned_dir;
}

void note_crossing(const Memory& m, int cell, int dir) {
    if (cell < 0 || m.portal_id[cell][dir] < 0)
        return;
    S.pending = true;
    S.pend_cell = cell;
    S.pend_dir = dir;
    S.pend_portal = m.portal_id[cell][dir];
}

} // namespace bot::nav::scout