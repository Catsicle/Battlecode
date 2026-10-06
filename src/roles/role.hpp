#pragma once

#include "../comms/comms.hpp"
#include "../core/memory.hpp"
#include "../nav/grid.hpp"

namespace bot::roles {

struct Action {
    bool split = false;
    int split_size = 0;
    struct PlanRequest {
        NavMode mode = NavMode::Explore;
        bool has_target = false;
        Vec2 target{};
    } request;
};

class Role {
  public:
    virtual ~Role() = default;
    virtual Action act(const unswbc::Controller&, const Memory&, const nav::Grid&, const comms::Inbox&) = 0;
};

} // namespace bot::roles
