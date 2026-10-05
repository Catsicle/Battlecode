# Agent handoff

- Do not edit `helper.hpp`.
- `main.cpp` owns only initialization, per-turn updates, role invocation, action
  emission, and `end_turn`.
- `Memory` owns persistent observations and is allocated once at map size.
- `comms/msg.hpp` is the only place that should change when sonar formats are
  specified.
- Direction order is NORTH, EAST, SOUTH, WEST.
- `Auto` must remain equivalent to `Stub` until explicit role assignment exists.
- Symmetry inference is disabled.
- Outgoing sonar is disabled.
- Preserve the final split call as `do_split(length - 2)`.
