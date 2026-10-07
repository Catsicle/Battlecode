#pragma once

#include "../comms/comms.hpp"
#include "../core/memory.hpp"
#include "../nav/grid.hpp"
#include "../core/body_tracker.hpp"

namespace bot::roles {

struct Action {
    // Splitting Command
    bool do_split = false;
    int split_size = 0;

    // Navigation Request
    struct PlanRequest {
        NavMode mode = NavMode::Explore;
        bool has_target = false;
        Vec2 target{};
    } request;
};

class Role {
  public:
    virtual ~Role() = default;
    virtual Action act(const unswbc::Controller&, const Memory&, const nav::Grid&, const comms::Inbox&,const bot::BodyTracker& ) = 0;
};

} // namespace bot::roles