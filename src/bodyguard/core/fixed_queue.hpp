#pragma once

#include <array>
#include <cstddef>

namespace bot::bodyguard::core {

template <typename Value, std::size_t Capacity>
class FixedQueue {
    static_assert(Capacity > 0);

    std::array<Value, Capacity> values{};
    std::size_t head = 0;
    std::size_t tail = 0;
    std::size_t size = 0;

  public:
    constexpr void clear() {
        head = 0;
        tail = 0;
        size = 0;
    }

    constexpr bool empty() const { return size == 0; }
    constexpr bool full() const { return size == Capacity; }
    constexpr std::size_t count() const { return size; }

    constexpr bool push(const Value& value) {
        if (full())
            return false;
        values[tail] = value;
        tail = (tail + 1) % Capacity;
        ++size;
        return true;
    }

    constexpr bool pop(Value& value) {
        if (empty())
            return false;
        value = values[head];
        head = (head + 1) % Capacity;
        --size;
        return true;
    }
};

} // namespace bot::bodyguard::core
