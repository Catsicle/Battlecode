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

void mark_seen(Memory& m, Vec2 p) {
    m.seen[m.index(p)] = true;
}

void mark_edge_known(Memory& m, Vec2 p, int d) {
    m.known_edges[m.index(p)] |= static_cast<std::uint8_t>(1U << d);
}

void mark_open(Memory& m, Vec2 p, int d) {
    int c = m.index(p);

    mark_edge_known(m, p, d);
    m.kelp[c] &= static_cast<std::uint8_t>(~(1U << d));
    m.portal_id[c][d] = -1;
}

void mark_blocked(Memory& m, Vec2 p, int d) {
    int c = m.index(p);

    mark_edge_known(m, p, d);
    m.kelp[c] |= static_cast<std::uint8_t>(1U << d);
    m.portal_id[c][d] = -1;
}

void make_unknown(Memory& m, Vec2 p, int d) {
    int c = m.index(p);

    m.known_edges[c] &= static_cast<std::uint8_t>(~(1U << d));
    m.kelp[c] &= static_cast<std::uint8_t>(~(1U << d));
    m.portal_id[c][d] = -1;
    m.portal_exit_known[c][d] = false;
}

void connect_open(Memory& m, Vec2 a, int d) {
    Vec2 b = step(m, a, d);

    mark_seen(m, a);
    mark_seen(m, b);

    mark_open(m, a, d);
    mark_open(m, b, opposite(d));
}

void place_portal(Memory& m, Vec2 p, int d, int pid) {
    int c = m.index(p);

    mark_seen(m, p);
    mark_edge_known(m, p, d);

    // A portal is not KELP.
    m.kelp[c] &= static_cast<std::uint8_t>(~(1U << d));

    m.portal_id[c][d] = static_cast<std::int16_t>(pid);
    m.portal_exit_known[c][d] = true;
}

void close_chamber(Memory& m, const std::vector<Vec2>& chamber) {
    std::vector<bool> inside(m.cells, false);

    for (Vec2 p : chamber) {
        inside[m.index(p)] = true;
        mark_seen(m, p);
    }

    for (Vec2 p : chamber) {
        for (int d = 0; d < 4; ++d) {
            Vec2 q = step(m, p, d);

            if (inside[m.index(q)])
                mark_open(m, p, d);
            else
                mark_blocked(m, p, d);
        }
    }
}

void reveal_unknown_tile(Memory& m, Vec2 tile) {
    mark_seen(m, tile);

    for (int d = 0; d < 4; ++d)
        mark_blocked(m, tile, d);

    // Find a known tile whose unknown edge points to this tile.
    for (int c = 0; c < m.cells; ++c) {
        if (!m.known(c))
            continue;

        Vec2 p = m.position(c);

        for (int d = 0; d < 4; ++d) {
            if (!m.edge_known(c, d) &&
                step(m, p, d).x == tile.x &&
                step(m, p, d).y == tile.y) {

                mark_open(m, p, d);
                mark_open(m, tile, opposite(d));
                return;
            }
        }
    }
}

void place_dragon(Memory& m, Vec2 p, int id = 99) {
    mark_seen(m, p);
    m.occupancy[m.index(p)] = {true, true, 123, id, 1};
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

/*
    MAP 1 - based on the first screenshot:
    long winding corridor / chamber.
    It extends beyond depth 5, so the portal is UNSAFE.
*/
void test_map1_winding() {
    std::cout << "\n=== MAP 1: Winding chamber ===\n";

    Memory m;
    m.initialize(20, 15);

    const int pid = 101;
    const Vec2 exit_portal{10, 9};
    const Vec2 enter_portal{1, 1};

    std::vector<Vec2> chamber = {
        {10,9},
        {9,9},{8,9},{7,9},
        {7,8},{7,7},{7,6},
        {6,6},{5,6},{4,6},
        {4,5},{4,4},{3,4},
        {2,4},{2,3}
    };

    close_chamber(m, chamber);

    place_portal(m, exit_portal, S, pid);
    place_portal(m, enter_portal, N, pid);

    auto result = bot::explore_portal(m, pid, exit_portal);

    expect(result, {-1, exit_portal.x, exit_portal.y});

    std::cout << "PASS: winding structure extends beyond depth 5\n";
}

/*
    MAP 2 - based on the second screenshot:
    compact irregular enclosed region.
    The other endpoint is deliberately far away / unrelated.
    This one is SAFE.
*/
void test_map2_compact_safe() {
    std::cout << "\n=== MAP 2: Compact enclosed chamber ===\n";

    Memory m;
    m.initialize(24, 18);

    const int pid = 202;
    const Vec2 exit_portal{12, 9};
    const Vec2 enter_portal{3, 15};

    std::vector<Vec2> chamber = {
        {12,9},{13,9},{14,9},
        {13,8},{14,8},
        {14,10},{15,10},
        {14,11}
    };

    close_chamber(m, chamber);

    place_portal(m, exit_portal, E, pid);
    place_portal(m, enter_portal, W, pid);

    auto result = bot::explore_portal(m, pid, exit_portal);

    expect(
        result,
        {1,
         exit_portal.x, exit_portal.y,
         enter_portal.x, enter_portal.y}
    );

    std::cout << "PASS: compact chamber is fully bounded\n";
}

/*
    MAP 3 - based on the third screenshot:
    irregular chamber with a dragon visible nearby.
    The dragon is placed OUTSIDE the chamber, so it must not alter
    the chamber-safety result.
*/
void test_map3_irregular_with_dragon() {
    std::cout << "\n=== MAP 3: Irregular chamber + nearby dragon ===\n";

    Memory m;
    m.initialize(24, 18);

    const int pid = 303;
    const Vec2 exit_portal{11, 10};
    const Vec2 enter_portal{20, 2};

    std::vector<Vec2> chamber = {
        {11,10},
        {10,10},{9,10},
        {9,9},{10,9},
        {9,8},{8,8},{7,8},
        {7,9},{7,10},
        {8,10},
        {7,11},{8,11},
        {8,12}
    };

    close_chamber(m, chamber);

    // Dragon visible beside, but not inside, the chamber.
    place_dragon(m, {12,10}, 404);

    place_portal(m, exit_portal, W, pid);
    place_portal(m, enter_portal, E, pid);

    auto result = bot::explore_portal(m, pid, exit_portal);

    expect(
        result,
        {1,
         exit_portal.x, exit_portal.y,
         enter_portal.x, enter_portal.y}
    );

    std::cout << "PASS: nearby dragon does not change bounded-chamber result\n";
}

/*
    MAP 4 - realistic multi-turn situation:
    an unknown frontier exists inside an otherwise winding chamber.
    First call must return the unknown tile.
    After that tile is revealed, the same function must conclude.
*/
void test_map4_multiturn_unknown() {
    std::cout << "\n=== MAP 4: Winding chamber, UNKNOWN -> SAFE ===\n";

    Memory m;
    m.initialize(24, 18);

    const int pid = 404;
    const Vec2 exit_portal{10, 10};
    const Vec2 enter_portal{21, 15};

    std::vector<Vec2> chamber = {
        {10,10},
        {9,10},{8,10},{8,9},
        {8,8},{9,8},{10,8},
        {10,7}
    };

    close_chamber(m, chamber);

    place_portal(m, exit_portal, N, pid);
    place_portal(m, enter_portal, S, pid);

    // Unknown continuation from (10,7) to (11,7).
    Vec2 parent{10,7};
    Vec2 unknown{11,7};

    make_unknown(m, parent, E);
    make_unknown(m, unknown, W);

    auto first = bot::explore_portal(m, pid, exit_portal);

    expect(first, {unknown.x, unknown.y});

    // Simulate next turn after exploring the returned unknown tile.
    reveal_unknown_tile(m, unknown);

    auto second = bot::explore_portal(m, pid, exit_portal);

    expect(
        second,
        {1,
         exit_portal.x, exit_portal.y,
         enter_portal.x, enter_portal.y}
    );

    std::cout << "PASS: multi-turn exploration works\n";
}

/*
    MAP 5 - same visual style as the irregular maps, but with
    another portal inside the chamber.
    Must be UNSAFE immediately.
*/
void test_map5_extra_portal() {
    std::cout << "\n=== MAP 5: Irregular chamber + extra portal ===\n";

    Memory m;
    m.initialize(24, 18);

    const int pid = 505;
    const int extra_pid = 606;

    const Vec2 exit_portal{10, 10};
    const Vec2 enter_portal{20, 2};

    std::vector<Vec2> chamber = {
        {10,10},
        {9,10},{8,10},
        {8,9},{8,8},
        {9,8},{10,8},
        {10,9},{11,9}
    };

    close_chamber(m, chamber);

    place_portal(m, exit_portal, E, pid);
    place_portal(m, enter_portal, W, pid);

    // Extra portal inside the same chamber.
    place_portal(m, {9,8}, N, extra_pid);

    auto result = bot::explore_portal(m, pid, exit_portal);

    expect(result, {-1, exit_portal.x, exit_portal.y});

    std::cout << "PASS: extra portal makes chamber unsafe\n";
}

/*
    MAP 6 - wrapping edge case:
    the chamber deliberately crosses the left/right map boundary.
*/
void test_map6_wrapping() {
    std::cout << "\n=== MAP 6: Wrapped chamber ===\n";

    Memory m;
    m.initialize(10, 10);

    const int pid = 707;
    const Vec2 exit_portal{0, 5};
    const Vec2 enter_portal{7, 7};

    std::vector<Vec2> chamber = {
        {0,5},
        {9,5},
        {8,5},
        {8,4},
        {9,4}
    };

    close_chamber(m, chamber);

    place_portal(m, exit_portal, W, pid);
    place_portal(m, enter_portal, E, pid);

    auto result = bot::explore_portal(m, pid, exit_portal);

    expect(
        result,
        {1,
         exit_portal.x, exit_portal.y,
         enter_portal.x, enter_portal.y}
    );

    std::cout << "PASS: wrapping handled correctly\n";
}

} // namespace

int main() {
    test_map1_winding();
    test_map2_compact_safe();
    test_map3_irregular_with_dragon();
    test_map4_multiturn_unknown();
    test_map5_extra_portal();
    test_map6_wrapping();

    std::cout << "\nALL VISUAL-MAP TESTS PASSED\n";
    return 0;
}
