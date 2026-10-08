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
    return {wrap(p.x + dx[d], m.width), wrap(p.y + dy[d], m.height)};
}

int opposite(int d) {
    return (d + 2) % 4;
}

void mark_seen(Memory& m, Vec2 p) {
    m.seen[m.index(p)] = true;
}

void set_edge_known(Memory& m, Vec2 p, int d) {
    m.known_edges[m.index(p)] |= static_cast<std::uint8_t>(1U << d);
}

void set_open(Memory& m, Vec2 p, int d) {
    set_edge_known(m, p, d);
    m.kelp[m.index(p)] &= static_cast<std::uint8_t>(~(1U << d));
}

void set_blocked(Memory& m, Vec2 p, int d) {
    set_edge_known(m, p, d);
    m.kelp[m.index(p)] |= static_cast<std::uint8_t>(1U << d);
}

void set_unknown(Memory& m, Vec2 p, int d) {
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
    set_open(m, a, d);
    set_open(m, b, opposite(d));
}

void place_portal(Memory& m, Vec2 p, int d, int pid) {
    mark_seen(m, p);
    set_edge_known(m, p, d);
    m.portal_id[m.index(p)][d] = static_cast<std::int16_t>(pid);
    m.portal_exit_known[m.index(p)][d] = true;
}

void close_chamber(Memory& m, const std::vector<Vec2>& chamber) {
    std::vector<bool> inside(m.cells, false);

    for (Vec2 p : chamber) {
        inside[m.index(p)] = true;
        mark_seen(m, p);
    }

    for (Vec2 p : chamber) {
        for (int d = 0; d < 4; ++d) {
            if (inside[m.index(step(m, p, d))])
                set_open(m, p, d);
            else
                set_blocked(m, p, d);
        }
    }
}

void reveal_unknown_tile(Memory& m, Vec2 tile) {
    mark_seen(m, tile);

    for (int d = 0; d < 4; ++d)
        set_blocked(m, tile, d);

    for (int c = 0; c < m.cells; ++c) {
        if (!m.known(c)) continue;

        Vec2 p = m.position(c);

        for (int d = 0; d < 4; ++d) {
            if (!m.edge_known(c, d) &&
                step(m, p, d).x == tile.x &&
                step(m, p, d).y == tile.y) {
                set_open(m, p, d);
                set_open(m, tile, opposite(d));
                return;
            }
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

void expect(
    const std::vector<int>& got,
    const std::vector<int>& wanted)
{
    std::cout << "got      = " << show(got) << '\n';
    std::cout << "expected = " << show(wanted) << '\n';
    assert(got == wanted);
}

void test_safe_bounded() {
    std::cout << "\n[TEST 1] Safe chamber, bounded within depth 5\n";

    Memory m;
    m.initialize(20, 20);

    std::vector<Vec2> chamber = {
        {4, 4}, {5, 4}, {6, 4},
        {4, 5}, {5, 5}, {6, 5},
        {5, 6}
    };

    close_chamber(m, chamber);

    int pid = 7;
    Vec2 exit{4, 4};
    Vec2 enter{15, 15};

    place_portal(m, exit, E, pid);
    place_portal(m, enter, W, pid);

    expect(
        bot::explore_portal(m, pid, exit),
        {1, exit.x, exit.y, enter.x, enter.y}
    );

    std::cout << "PASS\n";
}

void test_unknown_multiturn() {
    std::cout << "\n[TEST 2] Unknown tile, then SAFE on next turn\n";

    Memory m;
    m.initialize(20, 20);

    std::vector<Vec2> chamber = {
        {4, 4}, {5, 4}, {6, 4},
        {4, 5}, {5, 5}, {6, 5}
    };

    close_chamber(m, chamber);

    int pid = 11;
    Vec2 exit{4, 4};
    Vec2 enter{16, 16};

    place_portal(m, exit, E, pid);
    place_portal(m, enter, W, pid);

    // Remove the E edge of (6,5), creating an unknown continuation.
    Vec2 parent{6, 5};
    Vec2 unknown{7, 5};
    set_unknown(m, parent, E);
    set_unknown(m, unknown, W);

    auto r1 = bot::explore_portal(m, pid, exit);

    std::cout << "turn 1:\n";
    expect(r1, {unknown.x, unknown.y});

    // Next turn: the bot explored that tile and Memory was updated.
    reveal_unknown_tile(m, unknown);

    auto r2 = bot::explore_portal(m, pid, exit);

    std::cout << "turn 2:\n";
    expect(
        r2,
        {1, exit.x, exit.y, enter.x, enter.y}
    );

    std::cout << "PASS\n";
}

void test_extra_portal_unsafe() {
    std::cout << "\n[TEST 3] Extra portal -> UNSAFE\n";

    Memory m;
    m.initialize(20, 20);

    std::vector<Vec2> chamber = {
        {4, 4}, {5, 4}, {6, 4},
        {4, 5}, {5, 5}, {6, 5}
    };

    close_chamber(m, chamber);

    int pid = 21;
    Vec2 exit{4, 4};
    Vec2 enter{16, 16};

    place_portal(m, exit, E, pid);
    place_portal(m, enter, W, pid);

    // Different portal inside the chamber.
    place_portal(m, {6, 4}, S, 99);

    expect(
        bot::explore_portal(m, pid, exit),
        {-1, exit.x, exit.y}
    );

    std::cout << "PASS\n";
}

void test_beyond_depth5_unsafe() {
    std::cout << "\n[TEST 4] Open path beyond depth 5 -> UNSAFE\n";

    Memory m;
    m.initialize(30, 20);

    int pid = 31;
    Vec2 exit{2, 2};
    Vec2 enter{20, 15};

    // Make a straight known-open corridor from x=2 to x=8.
    for (int x = 2; x <= 8; ++x)
        mark_seen(m, {x, 2});

    for (int x = 2; x < 8; ++x)
        connect_open(m, {x, 2}, E);

    // Close N/S edges of the corridor.
    for (int x = 2; x <= 8; ++x) {
        set_blocked(m, {x, 2}, N);
        set_blocked(m, {x, 2}, S);
    }

    // End walls.
    set_blocked(m, {8, 2}, E);
    set_blocked(m, {2, 2}, W);

    place_portal(m, exit, W, pid);
    place_portal(m, enter, E, pid);

    // From x=2, cells x=3..8 are depth 1..6.
    // Therefore the chamber is not bounded within depth 5.
    expect(
        bot::explore_portal(m, pid, exit),
        {-1, exit.x, exit.y}
    );

    std::cout << "PASS\n";
}

void test_portal_id_selection() {
    std::cout << "\n[TEST 5] Supplied portal ID selects the correct chamber\n";

    Memory m;
    m.initialize(30, 30);

    // Target chamber: SAFE.
    std::vector<Vec2> target = {
        {3, 3}, {4, 3},
        {3, 4}, {4, 4}
    };
    close_chamber(m, target);

    // Other chamber: contains another portal and is UNSAFE.
    std::vector<Vec2> other = {
        {20, 20}, {21, 20},
        {20, 21}, {21, 21}
    };
    close_chamber(m, other);

    int target_pid = 50;
    int other_pid = 80;

    Vec2 target_exit{3, 3};
    Vec2 target_enter{25, 25};

    Vec2 other_exit{20, 20};
    Vec2 other_enter{27, 27};

    place_portal(m, target_exit, E, target_pid);
    place_portal(m, target_enter, W, target_pid);

    place_portal(m, other_exit, E, other_pid);
    place_portal(m, other_enter, W, other_pid);

    // Extra portal belongs to the other chamber.
    place_portal(m, {21, 20}, S, 123);

    // Ask for target_pid only.
    expect(
        bot::explore_portal(m, target_pid, target_exit),
        {1, target_exit.x, target_exit.y,
         target_enter.x, target_enter.y}
    );

    std::cout << "PASS\n";
}

} // namespace

int main() {
    test_safe_bounded();
    test_unknown_multiturn();
    test_extra_portal_unsafe();
    test_beyond_depth5_unsafe();
    test_portal_id_selection();

    std::cout << "\nALL TESTS PASSED\n";
    return 0;
}
