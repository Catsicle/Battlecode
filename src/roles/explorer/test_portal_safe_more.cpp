#include "portal_safe.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using bot::Memory;
using bot::Vec2;

namespace {

constexpr int N = 0;
constexpr int E = 1;
constexpr int S = 2;
constexpr int W = 3;

int wrap(int v, int n) {
    v %= n;
    if (v < 0) v += n;
    return v;
}

Vec2 step(const Memory& m, Vec2 p, int d) {
    static constexpr int dx[4] = {0, 1, 0, -1};
    static constexpr int dy[4] = {-1, 0, 1, 0};

    return {
        wrap(p.x + dx[d], m.width),
        wrap(p.y + dy[d], m.height)
    };
}

int opposite(int d) {
    return (d + 2) % 4;
}

void seen(Memory& m, Vec2 p) {
    m.seen[m.index(p)] = true;
}

void known_edge(Memory& m, Vec2 p, int d) {
    m.known_edges[m.index(p)] |= static_cast<std::uint8_t>(1U << d);
}

void open_edge(Memory& m, Vec2 p, int d) {
    known_edge(m, p, d);
    m.kelp[m.index(p)] &= static_cast<std::uint8_t>(~(1U << d));
}

void blocked_edge(Memory& m, Vec2 p, int d) {
    known_edge(m, p, d);
    m.kelp[m.index(p)] |= static_cast<std::uint8_t>(1U << d);
}

void unknown_edge(Memory& m, Vec2 p, int d) {
    int c = m.index(p);
    m.known_edges[c] &= static_cast<std::uint8_t>(~(1U << d));
    m.kelp[c] &= static_cast<std::uint8_t>(~(1U << d));
    m.portal_id[c][d] = -1;
    m.portal_exit_known[c][d] = false;
}

void connect_open(Memory& m, Vec2 a, int d) {
    Vec2 b = step(m, a, d);
    seen(m, a);
    seen(m, b);
    open_edge(m, a, d);
    open_edge(m, b, opposite(d));
}

void place_portal(Memory& m, Vec2 p, int d, int pid) {
    seen(m, p);
    known_edge(m, p, d);
    m.portal_id[m.index(p)][d] = static_cast<std::int16_t>(pid);
    m.portal_exit_known[m.index(p)][d] = true;
}

/*
    Makes every edge of the supplied cells known.
    Edges between chamber cells are OPEN.
    Edges leaving the chamber are BLOCKED.
*/
void close_chamber(Memory& m, const std::vector<Vec2>& chamber) {
    std::vector<bool> inside(m.cells, false);

    for (Vec2 p : chamber) {
        inside[m.index(p)] = true;
        seen(m, p);
    }

    for (Vec2 p : chamber) {
        for (int d = 0; d < 4; ++d) {
            Vec2 q = step(m, p, d);

            if (inside[m.index(q)])
                open_edge(m, p, d);
            else
                blocked_edge(m, p, d);
        }
    }
}

std::string show(const std::vector<int>& r) {
    std::string s = "{";

    for (std::size_t i = 0; i < r.size(); ++i) {
        if (i) s += ", ";
        s += std::to_string(r[i]);
    }

    return s + "}";
}

void expect(const std::vector<int>& got,
            const std::vector<int>& expected) {
    std::cout << "got      = " << show(got) << '\n';
    std::cout << "expected = " << show(expected) << '\n';
    assert(got == expected);
}

/* 6) Unknown edge immediately at the exit. */
void test_unknown_at_exit() {
    std::cout << "\n[TEST 6] Unknown immediately at EXIT\n";

    Memory m;
    m.initialize(12, 12);

    Vec2 exit{0, 0};
    Vec2 enter{8, 8};
    int pid = 60;

    seen(m, exit);
    for (int d = 0; d < 4; ++d)
        blocked_edge(m, exit, d);

    // Unknown NORTH edge from (0,0) wraps to (0,11).
    unknown_edge(m, exit, N);

    place_portal(m, exit, E, pid);
    place_portal(m, enter, W, pid);

    expect(
        bot::explore_portal(m, pid, exit),
        {0, 11}
    );

    std::cout << "PASS\n";
}

/* 7) Unknown exactly at depth 5. */
void test_unknown_at_depth5() {
    std::cout << "\n[TEST 7] Unknown at exactly depth 5\n";

    Memory m;
    m.initialize(20, 20);

    int pid = 61;
    Vec2 exit{2, 2};
    Vec2 enter{15, 15};

    std::vector<Vec2> corridor;
    for (int x = 2; x <= 7; ++x)
        corridor.push_back({x, 2});

    close_chamber(m, corridor);

    // The last known tile is depth 5 from the exit.
    Vec2 p{7, 2};

    // Replace its east boundary with an UNKNOWN edge.
    unknown_edge(m, p, E);

    place_portal(m, exit, W, pid);
    place_portal(m, enter, E, pid);

    expect(
        bot::explore_portal(m, pid, exit),
        {8, 2}
    );

    std::cout << "PASS\n";
}

/* 8) Safe exactly at depth 5: all boundary edges blocked. */
void test_safe_exact_depth5() {
    std::cout << "\n[TEST 8] SAFE with reachable cell at depth 5\n";

    Memory m;
    m.initialize(20, 20);

    int pid = 62;
    Vec2 exit{2, 2};
    Vec2 enter{16, 16};

    std::vector<Vec2> corridor;
    for (int x = 2; x <= 7; ++x)
        corridor.push_back({x, 2});

    close_chamber(m, corridor);

    place_portal(m, exit, W, pid);
    place_portal(m, enter, E, pid);

    expect(
        bot::explore_portal(m, pid, exit),
        {1, exit.x, exit.y, enter.x, enter.y}
    );

    std::cout << "PASS\n";
}

/* 9) Branching chamber, but everything is bounded within depth 5. */
void test_branching_safe() {
    std::cout << "\n[TEST 9] Branching chamber fully bounded within depth 5\n";

    Memory m;
    m.initialize(20, 20);

    int pid = 63;
    Vec2 exit{10, 10};
    Vec2 enter{1, 1};

    std::vector<Vec2> chamber = {
        {10,10},
        {9,10},{8,10},{7,10},
        {10,9},{10,8},{10,7},
        {11,10},{12,10},{13,10},
        {10,11},{10,12},{10,13},
        {9,9},{11,9}
    };

    close_chamber(m, chamber);

    place_portal(m, exit, N, pid);
    place_portal(m, enter, S, pid);

    expect(
        bot::explore_portal(m, pid, exit),
        {1, exit.x, exit.y, enter.x, enter.y}
    );

    std::cout << "PASS\n";
}

/* 10) Another portal at depth 5 must make it UNSAFE. */
void test_extra_portal_at_depth5() {
    std::cout << "\n[TEST 10] Extra portal at depth 5 -> UNSAFE\n";

    Memory m;
    m.initialize(20, 20);

    int pid = 64;
    int extra_pid = 100;

    Vec2 exit{2, 2};
    Vec2 enter{16, 16};

    std::vector<Vec2> corridor;
    for (int x = 2; x <= 7; ++x)
        corridor.push_back({x, 2});

    close_chamber(m, corridor);

    place_portal(m, exit, W, pid);
    place_portal(m, enter, E, pid);

    // Extra portal placed on the depth-5 tile.
    place_portal(m, {7, 2}, S, extra_pid);

    expect(
        bot::explore_portal(m, pid, exit),
        {-1, exit.x, exit.y}
    );

    std::cout << "PASS\n";
}

/* 11) Wrapping movement must work correctly. */
void test_wrapping() {
    std::cout << "\n[TEST 11] Map wrapping\n";

    Memory m;
    m.initialize(8, 8);

    int pid = 65;
    Vec2 exit{0, 0};
    Vec2 enter{6, 6};

    // Chamber goes through the left/right wrap:
    // (0,0) -> (7,0) -> (6,0).
    std::vector<Vec2> chamber = {
        {0,0},
        {7,0},
        {6,0}
    };

    close_chamber(m, chamber);

    place_portal(m, exit, S, pid);
    place_portal(m, enter, N, pid);

    expect(
        bot::explore_portal(m, pid, exit),
        {1, exit.x, exit.y, enter.x, enter.y}
    );

    std::cout << "PASS\n";
}

/* 12) Invalid portal ID -> UNSAFE-style failure return. */
void test_bad_portal_id() {
    std::cout << "\n[TEST 12] Unknown portal ID\n";

    Memory m;
    m.initialize(10, 10);

    int real_pid = 77;
    int wrong_pid = 78;

    Vec2 exit{2, 2};
    Vec2 enter{7, 7};

    std::vector<Vec2> chamber = {
        {2,2},{3,2},{2,3}
    };

    close_chamber(m, chamber);

    place_portal(m, exit, E, real_pid);
    place_portal(m, enter, W, real_pid);

    expect(
        bot::explore_portal(m, wrong_pid, exit),
        {-1, exit.x, exit.y}
    );

    std::cout << "PASS\n";
}

/* 13) Exit coordinate doesn't belong to the supplied portal ID. */
void test_wrong_exit_coordinate() {
    std::cout << "\n[TEST 13] Wrong exit coordinate for portal ID\n";

    Memory m;
    m.initialize(10, 10);

    int pid = 88;

    Vec2 real_exit{2, 2};
    Vec2 enter{7, 7};
    Vec2 wrong_exit{4, 4};

    std::vector<Vec2> chamber = {
        {2,2},{3,2},{2,3}
    };

    close_chamber(m, chamber);

    place_portal(m, real_exit, E, pid);
    place_portal(m, enter, W, pid);

    expect(
        bot::explore_portal(m, pid, wrong_exit),
        {-1, wrong_exit.x, wrong_exit.y}
    );

    std::cout << "PASS\n";
}

/* 14) Single-cell chamber: immediately bounded -> SAFE. */
void test_single_cell_safe() {
    std::cout << "\n[TEST 14] Single-cell chamber\n";

    Memory m;
    m.initialize(10, 10);

    int pid = 90;

    Vec2 exit{5, 5};
    Vec2 enter{1, 1};

    close_chamber(m, {exit});

    place_portal(m, exit, E, pid);
    place_portal(m, enter, W, pid);

    expect(
        bot::explore_portal(m, pid, exit),
        {1, exit.x, exit.y, enter.x, enter.y}
    );

    std::cout << "PASS\n";
}

} // namespace

int main() {
    test_unknown_at_exit();
    test_unknown_at_depth5();
    test_safe_exact_depth5();
    test_branching_safe();
    test_extra_portal_at_depth5();
    test_wrapping();
    test_bad_portal_id();
    test_wrong_exit_coordinate();
    test_single_cell_safe();

    std::cout << "\nALL ADDITIONAL TESTS PASSED\n";
    return 0;
}
