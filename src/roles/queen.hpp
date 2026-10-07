#pragma once
#include "role.hpp"

namespace bot::roles {

class QueenRole : public Role {
public:
    Action act(const unswbc::Controller&, const Memory&, const nav::Grid&, const comms::Inbox&) override;
};

QueenRole& queen_role();

} // namespace bot::roles
