#include "body_tracker.hpp"

#include <algorithm>

namespace bot {

void BodyTracker::reset() {
    body.clear();
    ++stats.splits;
}

void BodyTracker::reconcile(const unswbc::Controller& controller, const Memory& memory) {
    std::deque<int> visible;
    int head = memory.index(controller.get_position());
    if (head >= 0)
        visible.push_back(head);
    for (Vec2 position : memory.own_body) {
        int cell = memory.index(position);
        if (cell >= 0 && cell != head)
            visible.push_back(cell);
    }
    if (body.empty()) {
        body = visible;
        ++stats.rebuilds;
        return;
    }
    bool mismatch = body.empty() || body.front() != head ||
                    static_cast<int>(body.size()) > controller.get_length();
    if (!mismatch) {
        for (int cell : visible) {
            if (std::find(body.begin(), body.end(), cell) == body.end()) {
                mismatch = true;
                break;
            }
        }
    }
    if (mismatch) {
        body = visible;
        ++stats.mismatches;
        ++stats.rebuilds;
    }
    while (static_cast<int>(body.size()) > controller.get_length())
        body.pop_back();
}

void BodyTracker::predict(const std::deque<int>& next) {
    body = next;
}

} // namespace bot
