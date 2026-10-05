#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace bot::bodyguard::core {

template <std::size_t Capacity>
class FixedBitset {
    static constexpr std::size_t WordCount = (Capacity + 63U) / 64U;
    std::array<std::uint64_t, WordCount> words{};

  public:
    constexpr void clear() { words.fill(0); }

    constexpr void set(std::size_t index) {
        if (index >= Capacity)
            return;
        words[index / 64U] |= std::uint64_t{1} << (index % 64U);
    }

    constexpr void reset(std::size_t index) {
        if (index >= Capacity)
            return;
        words[index / 64U] &= ~(std::uint64_t{1} << (index % 64U));
    }

    constexpr bool test(std::size_t index) const {
        return index < Capacity && (words[index / 64U] & (std::uint64_t{1} << (index % 64U))) != 0;
    }

    constexpr std::size_t count() const {
        std::size_t result = 0;
        for (std::uint64_t word : words) {
#if defined(__GNUC__) || defined(__clang__)
            result += static_cast<std::size_t>(__builtin_popcountll(word));
#else
            while (word != 0) {
                word &= word - 1;
                ++result;
            }
#endif
        }
        return result;
    }
};

} // namespace bot::bodyguard::core
