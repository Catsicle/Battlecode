#pragma once

#include "transition.hpp"

#include <deque>

namespace bot::nav {

enum class Fail { OK, KELP, SELF, OTHER_BODY, HEAD_ON, PORTAL_UNRESOLVED, TRAP, DEAD_END };

struct Undo {
    int tile = -1;
    int old_stamp = 0;
    bool took_pearl = false;
    int old_head = -1;
    int old_length = 0;
    int old_e = 0;
    int old_counter = 0;
};

struct Sim {
    int L = 0;
    int counter = 0;
    int head = -1;
    int e = 0;
    std::vector<int> stamp;
    std::vector<std::uint8_t> pearl_taken;

    void reset(const std::deque<int>& body, int length);
    int seg_index(int tile) const;
    Fail try_step(const Memory& memory, int direction, int rho, Undo& undo) ;
    void undo(const Undo& undo);
};

} // namespace bot::nav
