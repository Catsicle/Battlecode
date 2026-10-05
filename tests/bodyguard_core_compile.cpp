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
    if (!cells.test(4095) || cells.test(0) || cells.count() != 1)
        return 1;

    bot::Memory memory;
    memory.initialize(10, 10);
    memory.seen[0] = true;
    memory.known_edges[0] = 0x0f;
    memory.kelp[0] = 1U << 0;
    memory.portal_id[0][1] = 42;
    memory.portal_exit_known[0][1] = true;
    memory.pearl[0] = true;
    memory.occupancy[0].present = true;

    MapView view(memory);
    if (view.neighbor(0, bot::Dir::NORTH) != 90)
        return 1;
    if (view.neighbor(0, bot::Dir::WEST) != 9)
        return 1;
    if (!view.known(0) || !view.frontier(0))
        return 1;
    if (!view.edge_known(0, 0) || !view.kelp(0, 0))
        return 1;
    if (view.portal_id(0, 1) != 42 || !view.portal_exit_known(0, 1))
        return 1;
    if (!view.has_pearl(0) || !view.occupancy(0).present)
        return 1;
    return 0;
}