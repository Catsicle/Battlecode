# Agent handoff

- Do not edit `helper.hpp`.
- `main.cpp` owns only initialization, per-turn updates, role invocation, action
  emission, and `end_turn`.
- `Memory` owns persistent observations and is allocated once at map size.
- `Memory::initialize()` resets map storage and transient movement history; do
  not reuse `previous_head` or pearl-visit state across map initialization.
- `Memory::update()` records observed pearl timers, kelp edges, portal IDs,
  occupancy, and own-body tiles. A portal exit is marked known only when both
  matching endpoints are visible in the same observation.
- Occupancy records retain their last-observed round. Treat age conservatively
  until a navigation safety layer explicitly models stale bodies; clearing old
  occupancy as free space caused smoke-game regressions.
- `comms/msg.hpp` is the only place that should change when sonar formats are
  specified.
- Direction order is NORTH, EAST, SOUTH, WEST.
- `Auto` must remain equivalent to `Stub` until explicit role assignment exists.
- Symmetry inference is disabled.
- Outgoing sonar is disabled.
- Preserve the final split call as `do_split(length - 2)`.
