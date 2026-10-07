#pragma once

#include <array>
#include "../core/types.hpp"

namespace bot {

class CycleDetector {
    static constexpr int HISTORY = 32;

    std::array<Vec2, HISTORY> history{};
    int size = 0;
    int write_pos = 0;

    static bool equal(const Vec2& a, const Vec2& b) {
        return a.x == b.x && a.y == b.y;
    }

    const Vec2& from_back(int offset) const {
        int idx = write_pos - 1 - offset;
        while (idx < 0) {
            idx += HISTORY;
        }
        return history[idx];
    }

public:
    void reset() {
        size = 0;
        write_pos = 0;
    }

    void push(const Vec2& position) {
        history[write_pos] = position;
        write_pos = (write_pos + 1) % HISTORY;
        if (size < HISTORY) {
            ++size;
        }
    }

    bool detect() const {
        for (int period = 2; period <= 16; ++period) {
            if (size < 2 * period) {
                continue;
            }
            bool repeated = true;
            for (int i = 0; i < period; ++i) {
                if (!equal(from_back(i), from_back(i + period))) {
                    repeated = false;
                    break;
                }
            }
            if (repeated) {
                return true;
            }
        }
        return false;
    }
};

} // namespace bot
