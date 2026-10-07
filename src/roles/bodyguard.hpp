#pragma once

#include "role.hpp"
#include "ring_path.hpp"

namespace bot::roles {

enum class BodyguardState {
    NeedRing,
    EnteringRing,
    Tracing,
    Dying
};

class BodyguardRole final : public Role {
    BodyguardState state = BodyguardState::NeedRing;

    RingPath path{};
    int current_index = 0;
    RingPath best_paths[10];
    int best_path_count = 0;
    int dir_sign = 1;            // +1 clockwise, -1 anticlockwise
    int search_start_round = -1;
    bool has_pending = false;
    Vec2 pending_target{};
    int ring_score = 0;


public:
    void reset();

    bool build_ring(
        const unswbc::Controller& controller,
        const Memory& memory,
        int round
    );

    int locate(const Vec2& p) const;
    Action trace(const unswbc::Controller&, const Memory&, const nav::Grid&, const comms::Inbox&);
    Action die(const unswbc::Controller&, const Memory&, const nav::Grid&, const comms::Inbox&);

    Action act(
        const unswbc::Controller& controller,
        const Memory& memory,
        const nav::Grid& grid,
        const comms::Inbox& inbox
    ) override;
};

Role& bodyguard_role();

} // namespace bot::roles
