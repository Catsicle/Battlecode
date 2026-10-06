# Navigation system specification

This document is the implementation specification for the Battlecode AU bot's
navigation system. It covers two behaviors:

- **Navigate:** move efficiently from the current position to a valid target.
- **Explore:** move through the known map, prefer useful or unexplored cells,
  and avoid cells that have already been visited without pearls.

The navigation system must be deterministic, safe, and compatible with the
organizer-provided protocol helper.

## Scope and fixed decisions

- All bot code belongs under `src/`.
- `src/helper.hpp` is organizer-provided and must not be modified.
- Sonar payload decoding is out of scope. Keep
  `comms/msg.hpp::decode_navigation_target` as a placeholder.
- `NAVIGATION_FIXTURE` and `FIXTURE_TARGET_X/Y` are the only Navigate target
  source for the first implementation.
- A valid navigation-target message applies to the receiving bot.
- `Auto` always selects the `Stub` role.
- The Stub role is the only implemented role. Other role files remain
  placeholders.
- Symmetry-based inference is disabled.
- A one-time `map_width * map_height` allocation is allowed during
  initialization. Search loops must reuse persistent storage.
- Direction ordering is always:
  `NORTH`, `EAST`, `SOUTH`, `WEST`.
- Unknown destinations are terminal for Navigate planning.
- Allies and enemies remain stationary in the planned search tree unless a
  safety rule explicitly accounts for their reach.
- The configured maximum-size rule and `SPLIT 2` fallback must be preserved.

## Source layout

```text
src/
  main.cpp                 # protocol loop only
  helper.hpp               # untouched organizer helper
  bot.toml                 # every source file must be listed
  config.hpp               # roles, constants, feature flags
  core/
    types.hpp              # coordinates, directions, paths
    memory.hpp/.cpp        # persistent map and observations
    body_tracker.hpp/.cpp  # ordered own-body prediction
    log.hpp                # compile-time logging macro
  comms/
    msg.hpp                # message types and auth placeholder
    comms.hpp/.cpp         # inbox/outbox plumbing
  nav/
    grid.hpp/.cpp          # map view used by navigation
    transition.hpp/.cpp    # single movement-transition implementation
    sim.hpp/.cpp           # reversible body simulation
    safety.hpp/.cpp        # L1, L3, and L4 validation
    astar.cpp              # weighted distance-field search
    bfs.cpp                # reachable-area utilities
    flee.cpp               # legacy safety utility
    wander.cpp             # Explore destination selection
    planner.hpp/.cpp       # PlanRequest to Path
    executor.hpp/.cpp      # validation, retry, and action emission
    dial.hpp               # optional queue, disabled initially
  roles/
    role.hpp               # role and PlanRequest interfaces
    registry.cpp            # role selection
    stub.cpp                # current real role
    explorer.cpp            # placeholder
    killer.cpp              # placeholder
    feeder.cpp              # placeholder
    bodyguard.cpp           # placeholder
    queen.cpp               # placeholder
```

## Runtime flow

Each turn follows this order:

1. Read the controller state and visible tiles.
2. Update persistent map, pearl, portal, and enemy-occupancy memory.
3. Reconcile the ordered own-body tracker with the new observation.
4. Read the communication inbox.
5. Ask the selected role for a `PlanRequest`; roles do not return finished
   paths.
6. Build a navigation grid over persistent memory.
7. Run one search from the current head.
8. Select a Navigate target or an Explore destination from that search field.
9. Validate candidate prefixes with the shared transition and simulation
   layers.
10. Emit the longest safe prefix accepted by the executor.
11. Apply the emitted `MoveRecord` to pearl and visit memory.
12. Predict the next ordered body state.
13. End the turn.

`main.cpp` should contain only this orchestration and protocol loop. Movement
rules must not be duplicated in `main.cpp`, roles, or individual search
algorithms.

## Persistent memory

`Memory` stores facts learned from the current 7x7 vision window and retains
them after they leave vision:

- known cells and known directional edges;
- kelp edges;
- portal endpoint observations;
- pearl state, pearl spawn timing, and cells known never to spawn pearls;
- visit counts and last visit rounds;
- fresh enemy occupancy;
- the previous head position and current visible own-body cells.

Enemy occupancy is valid only while:

```text
now - occupancy.round <= OCC_STALE_ROUNDS
```

Own segments are not treated as enemy occupancy. The own body is represented
by `BodyTracker`.

### Move bookkeeping

Before emitting a candidate path, create a `MoveRecord` containing:

- every traversed cell, in order;
- whether a pearl was known on each traversed cell before movement;
- the length before the move.

After the action, compute:

```text
eaten = length_after - length_before
```

Mark the first `eaten` pearl-flagged cells as eaten, increment
`visits_nopearl` for every other traversed cell, and update
`last_visit_round` for every traversed cell. Do not infer pearl consumption by
checking the head tile at the start of the next turn.

## Ordered own-body tracking

`BodyTracker::body` is a head-first deque that persists across turns.

After a validated move prefix, the tracker predicts the complete resulting
body from the simulation state. On the next turn it reconciles:

- predicted head versus observed head;
- predicted length versus controller length;
- visible own segments versus the predicted body restricted to vision.

On mismatch caused by pearl growth, damage, spawning, or splitting:

1. rebuild from the visible head and connected own segments;
2. use the prior prediction to resolve ambiguity;
3. retain the predicted suffix outside vision, truncated to the actual length;
4. increment mismatch/rebuild statistics.

After splitting, reset the tracker and do not use the pre-split body.

## Shared movement transition

Every planner and validator call must use:

```cpp
Step resolve_step(const Memory& memory, int tile, int direction);
```

The result contains the destination, resulting direction, and one of:

- `Normal`
- `PortalOK`
- `PortalUnresolved`
- `Kelp`

Rules:

- A known kelp edge returns `Kelp`.
- A normal known edge returns its wrapped neighbor.
- A portal is resolved only when both endpoints for its portal id are known.
- The portal exit tile, exit direction, and emergence tile are computed in one
  named portal-transition function.
- An unobserved exit edge returns `PortalUnresolved`.
- A known kelp exit returns `Kelp`.
- Navigate rejects unresolved portals.
- Explore may consider an unresolved portal only with
  `C_PORTAL_UNKNOWN` cost; it must still be rejected by safety validation if
  the actual destination cannot be established.

Any behavior marked `VERIFY` must be isolated behind one named function or
constant and documented in `docs/NOTES.md` after checking the official rules
and `helper.hpp`.

## Reversible simulation

`Sim` models only the controlled dragon and uses stamp arrays for O(1)-style
occupancy checks:

```text
L        current simulated length
counter  number of simulated entries
head     current head cell
e        pearls eaten in this simulation
stamp    entry stamp for each cell
```

On reset, the ordered body is stamped head-first. A destination is occupied
by the own body when its segment index is less than `L`; the tail counts as
occupied before the move. A successful move:

1. resolves the transition;
2. checks kelp and unresolved portals;
3. checks own-body occupancy;
4. checks fresh enemy occupancy;
5. records undo state;
6. stamps the destination;
7. grows for an unconsumed pearl or allows the tail to advance otherwise.

`undo()` must restore the exact prior state, including length, head, counter,
stamps, and pearl-consumption markers. The implementation must not allocate
inside recursive validation or the per-turn search loop.

## Safety validation

The executor validates each candidate prefix in this order.

### L1: exact immediate replay

Reject:

1. kelp;
2. unresolved portal transitions;
3. own-body collisions;
4. fresh enemy-body collisions;
5. head-on collisions using the configured enemy reach;
6. illegal or unknown destinations.

For known enemy segment metadata, use time-indexed age and segment index. For
unknown enemy segment metadata, treat the cell as blocked at `rho == 0`.

### L4: trap validation

After replaying a prefix, flood-fill the reachable non-kelp area while
respecting the simulated body. A candidate is acceptable when the reachable
area is at least:

```text
simulated_length + pearls_eaten + TRAP_EXTRA
```

or when it reaches a valid frontier that can continue exploration.

### L3: recursive survival

For the next `LOOKAHEAD_K` plies, recursively test whether at least one safe
continuation exists. Every recursive branch must use the same transition,
simulation, and undo operations as L1.

## Search and destination selection

The search starts once from the current head and fills a reusable weighted
distance field. `approach_target` and `wander` must not run a separate full
Dijkstra search for every candidate destination.

### Navigate

- If the target is known, choose the lowest-cost route to it.
- If it is unknown, select a reachable frontier that minimizes estimated
  toroidal distance to the target, then route cost, then path length.
- Re-run target search every turn.
- Keep the target fixed until reached.
- Once the target is reached, return to Explore mode.

### Explore

Score reachable cells using:

```text
S(v) = G(v) - D(v)
```

where `D(v)` is the distance-field cost and `G(v)` rewards pearls, valid
frontiers, stale-but-revisitable cells, and other configured exploration
benefits. Penalize cells according to `visits_nopearl`. Use
`last_visit_round`, not `last_seen_round`, for visit recency.

The search must preserve deterministic direction ordering and reject unknown
destinations in Navigate mode.

## Planner and executor

`PlanRequest` contains:

```cpp
NavMode mode;
bool has_target;
Vec2 target;
```

The Stub role produces Navigate requests for the fixture or a valid inbox
target, and Explore requests otherwise.

The executor retries in this exact order:

1. shorten the candidate prefix;
2. replan with the failed first edge forbidden;
3. choose the safest legal single move;
4. split only when no L1-safe move exists;
5. use the final legal fallback move.

The configured maximum-size rule must be respected. The first implementation
may emit one validated move per turn while exact engine replay behavior is
being verified; any multi-step emission must be based on the same body replay
used by validation.

## Configuration

All navigation constants belong in `src/config.hpp`:

```text
C_MAX=192
TRAP_EXTRA=2
MAX_RETRY=4
OCC_STALE_ROUNDS=3
DEFAULT_ENEMY_REACH=2
USE_DIAL=false
NAV_STATS=false
```

`USE_DIAL` gates an optional bounded-integer Dial queue. The default weighted
priority queue remains valid while the Dial implementation is disabled.

## Performance and reliability

- Target worst-case cost: below 15M points per turn on a 64x64 map.
- The judge limit is 100M points per turn.
- No stdout or iostream logging in release or hot paths.
- No per-node heap allocation inside search, recursive validation, or retry
  loops.
- Use generation stamps and persistent workspaces.
- Keep all movement semantics in `resolve_step`.
- Compile with warnings enabled and treat new warnings as defects.
- Register every added `.cpp` in `src/bot.toml`.

## Required validation

After each implementation phase:

1. compile the complete source list;
2. run all native tests;
3. run the Battlecode smoke match;
4. inspect the generated report and replay;
5. update `docs/NOTES.md` for every resolved assumption;
6. stop and fix failures before starting the next phase.

Tests must cover at least:

- ordered body reconstruction and mismatch handling;
- pearl growth and `MoveRecord` bookkeeping;
- apply/undo fuzzing;
- hairpin and tail collision behavior;
- fresh and stale enemy occupancy;
- portal endpoint pairing and unresolved portals;
- trap rejection;
- recursive survival;
- retry ordering;
- Navigate frontier progress;
- Explore scoring;
- split fallback;
- fixture integration and replay output.

