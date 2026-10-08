#include <cstdint>
#include <iostream>
#include <vector>

#include "comm_function.hpp"

int main()
{
    std::uint64_t round = 100;

    // =========================================================
    // 1. ENEMY QUEEN
    // =========================================================

    std::uint64_t queen_msg = enemy_queen_sonar(23, 41, round);
    auto queen = decode_sonar(queen_msg, round);

    std::cout << "Test 1 - Enemy Queen: ";
    for (auto x : queen)
        std::cout << x << " ";
    std::cout << "\n";


    // =========================================================
    // 2. ENEMY QUEEN - BOUNDARY COORDINATES
    // =========================================================

    std::uint64_t queen_boundary =
        enemy_queen_sonar(0, 63, round);

    auto queen_b = decode_sonar(queen_boundary, round);

    std::cout << "Test 2 - Queen Boundary: ";
    for (auto x : queen_b)
        std::cout << x << " ";
    std::cout << "\n";


    // =========================================================
    // 3. IDLE - SOME PEARLS
    // =========================================================

    std::vector<std::vector<bool>> pearl_grid(
        6, std::vector<bool>(6, false)
    );

    pearl_grid[0][0] = true;
    pearl_grid[2][3] = true;
    pearl_grid[5][5] = true;

    std::uint64_t idle_msg =
        idle_sonar(12, 34, pearl_grid, round);

    auto idle = decode_sonar(idle_msg, round);

    std::cout << "Test 3 - Idle: ";
    for (auto x : idle)
        std::cout << x << " ";
    std::cout << "\n";


    // =========================================================
    // 4. IDLE - NO PEARLS
    // =========================================================

    std::vector<std::vector<bool>> empty_grid(
        6, std::vector<bool>(6, false)
    );

    std::uint64_t idle_empty_msg =
        idle_sonar(0, 0, empty_grid, round);

    auto idle_empty =
        decode_sonar(idle_empty_msg, round);

    std::cout << "Test 4 - Idle No Pearls: ";
    for (auto x : idle_empty)
        std::cout << x << " ";
    std::cout << "\n";


    // =========================================================
    // 5. IDLE - ALL PEARLS
    // =========================================================

    std::vector<std::vector<bool>> full_grid(
        6, std::vector<bool>(6, true)
    );

    std::uint64_t idle_full_msg =
        idle_sonar(63, 63, full_grid, round);

    auto idle_full =
        decode_sonar(idle_full_msg, round);

    std::cout << "Test 5 - Idle All Pearls: ";
    for (auto x : idle_full)
        std::cout << x << " ";
    std::cout << "\n";


    // =========================================================
    // 6. PORTAL
    // =========================================================

    std::uint64_t portal_msg = portal_sonar(
        10, 20, 0,
        30, 40,
        15, 25, 1,
        50, 60, 2,
        round
    );

    auto portal = decode_sonar(portal_msg, round);

    std::cout << "Test 6 - Portal: ";
    for (auto x : portal)
        std::cout << x << " ";
    std::cout << "\n";


    // =========================================================
    // 7. PORTAL - BOUNDARY VALUES
    // =========================================================

    std::uint64_t portal_boundary_msg = portal_sonar(
        0, 0, 0,
        63, 63,
        0, 63, 1,
        63, 0, 2,
        round
    );

    auto portal_boundary =
        decode_sonar(portal_boundary_msg, round);

    std::cout << "Test 7 - Portal Boundary: ";
    for (auto x : portal_boundary)
        std::cout << x << " ";
    std::cout << "\n";


    // =========================================================
    // 8. SUICIDE
    // =========================================================

    std::uint64_t suicide_msg =
        suicide_sonar(round);

    auto suicide =
        decode_sonar(suicide_msg, round);

    std::cout << "Test 8 - Suicide: ";
    for (auto x : suicide)
        std::cout << x << " ";
    std::cout << "\n";


    // =========================================================
    // 9. QUEEN KILLED
    // =========================================================

    std::uint64_t killed_msg =
        queen_killed_sonar(round);

    auto killed =
        decode_sonar(killed_msg, round);

    std::cout << "Test 9 - Queen Killed: ";
    for (auto x : killed)
        std::cout << x << " ";
    std::cout << "\n";


    // =========================================================
    // 10. FOOD SIGNAL
    // =========================================================

    std::uint64_t food_msg =
        food_signal(23, 41, 150, round);

    auto food =
        decode_sonar(food_msg, round);

    std::cout << "Test 10 - Food Signal: ";
    for (auto x : food)
        std::cout << x << " ";
    std::cout << "\n";


    // =========================================================
    // 11. FOOD SIGNAL - MINIMUM VALUES
    // =========================================================

    std::uint64_t food_min_msg =
        food_signal(0, 0, 0, round);

    auto food_min =
        decode_sonar(food_min_msg, round);

    std::cout << "Test 11 - Food Minimum: ";
    for (auto x : food_min)
        std::cout << x << " ";
    std::cout << "\n";


    // =========================================================
    // 12. FOOD SIGNAL - MAXIMUM VALUES
    // =========================================================

    std::uint64_t food_max_msg =
        food_signal(63, 63, 255, round);

    auto food_max =
        decode_sonar(food_max_msg, round);

    std::cout << "Test 12 - Food Maximum: ";
    for (auto x : food_max)
        std::cout << x << " ";
    std::cout << "\n";


    // =========================================================
    // 13. PREVIOUS ROUND
    // =========================================================

    std::uint64_t previous_msg =
        enemy_queen_sonar(25, 30, round - 1);

    auto previous =
        decode_sonar(previous_msg, round);

    std::cout << "Test 13 - Previous Round: ";

    if (previous.empty())
        std::cout << "FAILED";
    else
        std::cout << "Accepted correctly";

    std::cout << "\n";


    // =========================================================
    // 14. WRONG ROUND
    // =========================================================

    auto wrong =
        decode_sonar(queen_msg, round + 5);

    std::cout << "Test 14 - Wrong Round: ";

    if (wrong.empty())
        std::cout << "Rejected correctly";
    else
        std::cout << "FAILED";

    std::cout << "\n";


    // =========================================================
    // 15. TAMPER AUTHENTICATION
    // =========================================================

    std::uint64_t tampered =
        queen_msg ^ 1ULL;

    auto tampered_result =
        decode_sonar(tampered, round);

    std::cout << "Test 15 - Tampered Authentication: ";

    if (tampered_result.empty())
        std::cout << "Rejected correctly";
    else
        std::cout << "FAILED";

    std::cout << "\n";


    // =========================================================
    // 16. TAMPER SUICIDE SECRET
    // =========================================================

    std::uint64_t bad_suicide =
        suicide_sonar(round);

    bad_suicide ^= (1ULL << 20);

    auto bad_suicide_result =
        decode_sonar(bad_suicide, round);

    std::cout << "Test 16 - Tampered Suicide: ";

    if (bad_suicide_result.empty())
        std::cout << "Rejected correctly";
    else
        std::cout << "FAILED";

    std::cout << "\n";


    return 0;
}