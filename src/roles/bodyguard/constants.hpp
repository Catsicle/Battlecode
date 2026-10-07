#pragma once
#include <algorithm>

namespace bodyguard_constants {

constexpr int DEFAULT_LQ = 8;
constexpr int MAX_DASH = 8;

constexpr int PEARL_HORIZON = 48;       // single source of truth (adapter + scorer)
constexpr int KEEP_MARGIN = 4;          // after a split: len == count - 4
constexpr int SPLIT_TRIGGER_MARGIN = 2; // split when projected_len >= count - 2
constexpr int MIN_CHILD = 2;
constexpr int SEARCH_PATIENCE = 12;     // rounds to hunt a "good" ring before settling
constexpr int SWITCH_HYSTERESIS = 15;   // score points
constexpr int MIN_PEARL_PCT = 70;       // replaces MIN_PATH_LENGTH_RATIO
constexpr int ROUND_LIMIT = 200;
constexpr int MAX_CYCLE_LENGTH = 128;
constexpr int MIN_INTERIOR_AREA = 16;

static_assert(KEEP_MARGIN + MIN_CHILD <= SPLIT_TRIGGER_MARGIN + KEEP_MARGIN, "");

inline int compute_steps(int Lq) {
    int free_steps = (Lq + 3) / 4;
    int s = std::max(2, (Lq + free_steps + 2) / 2);
    s = std::min(MAX_DASH, s);
    return s;
}

} // namespace bodyguard_constants
