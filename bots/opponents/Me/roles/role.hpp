#pragma once

#include <vector>

#include "../comms/comms.hpp"
#include "../core/memory.hpp"
#include "../nav/grid.hpp"
#include "../core/body_tracker.hpp"

namespace bot::roles {

struct SonarMessage {
    std::uint64_t North = 0;
    std::uint64_t East = 0;
    std::uint64_t South = 0;
    std::uint64_t West = 0;
};

struct Action {
    // Round number for sonar auth
    std::uint64_t round_num = 0;

    // Splitting Command
    bool do_split = false;
    int split_size = 0;

    // Sonar Emission
    SonarMessage sonar;

    // Navigation Request
    struct PlanRequest {
        NavMode mode = NavMode::Explore;
        bool has_target = false;
        Vec2 target{};
        std::vector<Dir> path;  // Vector path for Direct mode
    } request;
};

class Role {
  public:
    virtual ~Role() = default;
    virtual Action act(const unswbc::Controller&, const Memory&, const nav::Grid&, const comms::Inbox&,const bot::BodyTracker& ) = 0;
};

} // namespace bot::roles