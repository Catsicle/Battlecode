#pragma once

#include "memory.hpp"

#include <deque>

namespace bot {

struct BodyStats {
    int mismatches = 0;
    int rebuilds = 0;
    int splits = 0;
};

struct BodyTracker {
    std::deque<int> body;
    BodyStats stats;
    void reset();
    void reconcile(const unswbc::Controller&, const Memory&);
    void predict(const std::deque<int>& next);
};

} // namespace bot
