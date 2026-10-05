#include <iostream>
#include "comms.hpp"

using namespace std;

int main()
{
    uint64_t round = 100;

    // ---------------- ENEMY QUEEN ----------------
    uint64_t queen_msg = enemy_queen_sonar(23, 41, round);

    vector<uint64_t> queen = decode_sonar(queen_msg, round);

    cout << "Enemy Queen:\n";
    for (auto x : queen)
        cout << x << " ";
    cout << "\n\n";


    // ---------------- IDLE ----------------
    vector<vector<bool>> pearl_grid(6, vector<bool>(6, false));

    pearl_grid[0][0] = true;
    pearl_grid[2][3] = true;
    pearl_grid[5][5] = true;

    uint64_t idle_msg =
        idle_sonar(12, 34, pearl_grid, round);

    vector<uint64_t> idle =
        decode_sonar(idle_msg, round);

    cout << "Idle:\n";
    for (auto x : idle)
        cout << x << " ";
    cout << "\n\n";


    // ---------------- PORTAL ----------------
    uint64_t portal_msg = portal_sonar(
        10, 20, 0,       // start
        30, 40,          // point 1
        15, 25, 1,       // point 2
        50, 60, 2,       // point 3
        round
    );

    vector<uint64_t> portal =
        decode_sonar(portal_msg, round);

    cout << "Portal:\n";
    for (auto x : portal)
        cout << x << " ";
    cout << "\n\n";


    // ---------------- SUICIDE ----------------
    uint64_t suicide_msg = suicide_sonar(round);

    vector<uint64_t> suicide =
        decode_sonar(suicide_msg, round);

    cout << "Suicide:\n";
    for (auto x : suicide)
        cout << x << " ";
    cout << "\n\n";


    // ---------------- WRONG ROUND TEST ----------------
    vector<uint64_t> wrong =
        decode_sonar(queen_msg, round + 5);

    cout << "Wrong round:\n";
    if (wrong.empty())
        cout << "Rejected correctly\n";
    else
        cout << "ERROR: accepted\n";

    
    // ---------------- EXTRA TESTS ----------------

// 1. Queen: boundary coordinates
uint64_t q1 = enemy_queen_sonar(0, 0, round);
auto d1 = decode_sonar(q1, round);

cout << "Queen (0,0): ";
for (auto x : d1) cout << x << " ";
cout << "\n";


// 2. Queen: maximum coordinates
uint64_t q2 = enemy_queen_sonar(63, 63, round);
auto d2 = decode_sonar(q2, round);

cout << "Queen (63,63): ";
for (auto x : d2) cout << x << " ";
cout << "\n";


// 3. Idle: no pearls
vector<vector<bool>> empty_grid(6, vector<bool>(6, false));

uint64_t i1 = idle_sonar(0, 0, empty_grid, round);
auto di1 = decode_sonar(i1, round);

cout << "Idle no pearls: ";
for (auto x : di1) cout << x << " ";
cout << "\n";


// 4. Idle: all pearls
vector<vector<bool>> full_grid(6, vector<bool>(6, true));

uint64_t i2 = idle_sonar(63, 63, full_grid, round);
auto di2 = decode_sonar(i2, round);

cout << "Idle all pearls: ";
for (auto x : di2) cout << x << " ";
cout << "\n";


// 5. Portal: all directions
uint64_t p1 = portal_sonar(
    0, 0, 0,
    63, 63,
    0, 63, 1,
    63, 0, 2,
    round
);

auto dp1 = decode_sonar(p1, round);

cout << "Portal boundaries: ";
for (auto x : dp1) cout << x << " ";
cout << "\n";


// 6. Previous round should be accepted
uint64_t q3 = enemy_queen_sonar(25, 30, round - 1);

auto d3 = decode_sonar(q3, round);

cout << "Previous round: ";

if (d3.empty())
    cout << "REJECTED";
else {
    for (auto x : d3) cout << x << " ";
}

cout << "\n";


// 7. Tamper with one bit
uint64_t tampered = q1;
tampered ^= (1ULL << 40);

auto dt = decode_sonar(tampered, round);

cout << "Tampered message: ";

if (dt.empty())
    cout << "Rejected correctly";
else
    cout << "ERROR: accepted";

cout << "\n";


// 8. Wrong round should be rejected
auto wrong_round = decode_sonar(q1, round + 2);

cout << "Two rounds old/future: ";

if (wrong_round.empty())
    cout << "Rejected correctly";
else
    cout << "ERROR: accepted";

cout << "\n";


// 9. Tamper with suicide secret
uint64_t bad_suicide = suicide_sonar(round);
bad_suicide ^= (1ULL << 20);

auto ds = decode_sonar(bad_suicide, round);

cout << "Tampered suicide: ";

if (ds.empty())
    cout << "Rejected correctly";
else
    cout << "ERROR: accepted";

cout << "\n";

    return 0;
}