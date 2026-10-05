#pragma once

#include "../comms/comms.hpp"
#include "../core/memory.hpp"
#include "../nav/grid.hpp"

namespace bot::roles {

struct Action {
    bool split = false;
    int split_size = 0;
    Path path;
};

class Role {
  public:
    virtual ~Role() = default;
    virtual Action act(const unswbc::Controller&, const Memory&, const nav::Grid&, const comms::Inbox&) = 0;
};

} // namespace bot::roles
