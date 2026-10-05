#include "bodyguard/core/fixed_bitset.hpp"
#include "bodyguard/core/map_view.hpp"

using bot::bodyguard::core::FixedBitset;
using bot::bodyguard::core::MapView;

static_assert(sizeof(FixedBitset<4096>) == 512);

int main() {
    FixedBitset<4096> cells;
    cells.set(0);
    cells.set(4095);
    cells.reset(0);
    return cells.test(4095) && !cells.test(0) && cells.count() == 1 ? 0 : 1;
}