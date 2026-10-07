#pragma once
#include <functional>
#include <cmath>

namespace bot::roles::bodyguard {

struct BoxScore {
    int pearls_now = 0;
    int pearls_spawning = 0;
    int area = 0;
    int circumference = 0;
};

using BoxComparator = std::function<bool(const BoxScore&, const BoxScore&)>;

inline bool score_by_pearl_then_area(const BoxScore& a, const BoxScore& b) {
    int score_a = a.pearls_now + a.pearls_spawning;
    int score_b = b.pearls_now + b.pearls_spawning;
    if (score_a != score_b) return score_a > score_b;
    return a.area > b.area;
}

} // namespace bot::roles::bodyguard
