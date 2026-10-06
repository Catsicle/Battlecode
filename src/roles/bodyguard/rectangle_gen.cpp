#include "rect_adapter.hpp"
#include "../../core/memory.hpp"
#include <cstdio>

void test_integration(const bot::Memory& memory, int round) {
    bodyguard::rect::Snapshot S;
    bodyguard::rect_adapter::fill_snapshot(memory, round, S);

    // Example: generate up to 5 best boxes for a pocket at (10,10)
    bodyguard::rect::Box out[5];
    int n = bodyguard::rect::generate(S, 10, 10, 24, out, 5);

    // For smoke/debug only: print how many boxes were found
    printf("rect_ring adapter: W=%d H=%d boxes_found=%d\n", memory.width, memory.height, n);
}
