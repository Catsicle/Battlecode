#pragma once

#include "../../helper.hpp"

#include <string>

namespace bot {

enum class KillAction {
    NONE,
    KILL_QUEEN,
    KILL_ENEMY_HEAD,
    PREPARE_QUEEN
};

struct KillResult {
    KillAction action = KillAction::NONE;
    std::string path;       // e.g. "NWNWEE"
    unswbc::Position target{};
};

/*
    Local Queen-kill / preparation planner.

    Inputs:
      - current 7x7 Vision
      - visible enemy Queen head position
      - our ACTUAL dragon length

    Priority:
      1. Kill Queen this turn, if legally possible.
      2. Otherwise kill another enemy dragon HEAD this turn.
      3. Otherwise prepare for the Queen.

    Collision rules:
      - own body is blocked
      - Queen body is blocked
      - other enemy bodies are blocked
      - another enemy HEAD is blocked unless that exact head is the target
      - KELP edges are blocked
      - PORTAL edges are not used by this local planner

    Sprint / pearl rules:
      - first ceil(L / 4) steps are free, with L fixed at move start
      - after that, a step must be payable BEFORE the destination is checked
      - a normal paid step removes one segment
      - stepping onto a pearl grows the dragon by one and the tail does NOT
        advance, so that step removes no segment
      - therefore a pearl after the free portion can create more than just
        "one extra step": it both costs no segment and gives +1 length
      - a pearl cannot rescue an otherwise unpayable step, because the engine
        checks whether the dragon can pay before it checks/eats the destination
      - pearl tiles are one-time resources; the search remembers which visible
        pearls it has already eaten
      - a PAID + PEARL step has net length change 0:
            +1 for eating the pearl, -1 for paying the paid step

    The free-step count is fixed from the length at the START of the move.
    The current length is then updated after every successful step and is
    used to decide whether the next paid step can be afforded.
*/

KillResult kill(
    const unswbc::Vision& vision,
    unswbc::Position queen_head,
    int length);

} // namespace bot
