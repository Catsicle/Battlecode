#include "queen.hpp"

namespace bot::roles {

Action QueenRole::act(const unswbc::Controller&, const Memory&, const nav::Grid&, const comms::Inbox&) {
    Action action{}; // pure stub, default explore
    return action;
}

QueenRole& queen_role() {
    static QueenRole instance;
    return instance;
}

} // namespace bot::roles
