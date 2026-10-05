#pragma once
#include "torus.hpp"
namespace bot::bodyguard::core {
class MapView {
    const Memory& memory;
  public:
    explicit MapView(const Memory& source) : memory(source) {}
};
} // namespace bot::bodyguard::core
