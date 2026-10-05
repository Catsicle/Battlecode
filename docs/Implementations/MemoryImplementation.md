# Memory Implementation — Current State

## Source files
- `src/core/memory.hpp` (struct definition, adapter interface)
- `src/core/memory.cpp` (update logic, pearl tracking, portal pairing, occupancy)
- `src/bodyguard/core/types.hpp` (Cell, Position, Direction; added Cycle/CycleCandidate)
- Related usages: `sim.hpp`, `torus.hpp`, `map_view.hpp` (adapter reduced; queries moved to `src/core/memory/queries.cpp`/`.hpp`)

---

## 1. Memory Structure (`memory.hpp` lines 12–50)

Namespace `bot`. `Memory` is the persistent per-turn state container (not per-node heap; fixed-size vectors).

Fields and their roles:

- `width`, `height`, `cells`: Torus dimensions (`W×H`; `cells = W*H`). Set by `initialize()`.
- `kelp`: `vector<uint8_t>` length `cells`; bitmask per cell (4 directions). Set by `update()` when `edge.get_edge_type() == KELP`; bit `1 << d`.
- `known_edges`: Same format; set when any edge is observed (regardless of type). Provides `edge_known(cell, d)`.
- `portal_id`: `vector<array<int16_t,4>>`; stores portal ID per direction (`-1` = none). Only set when `edge.get_edge_type() == PORTAL`.
- `portal_exit_known`: `vector<array<bool,4>>`; true when both endpoints of a portal pair are visible (paired by `id`). Set by double-loop over `visible_portals` in `update()`.
- `seen`: `vector<bool>`; `true` when tile observed in vision window (`update()` sets `seen[cell]=true` for all `tile.get_position()` in controller vision). Not predictive.
- `last_seen_round`: `vector<int>`; tracks round of last observation.
- `pearl_spawn_round`: `vector<int>`; `max()` when `countdown < 0` (never spawns); else `round + countdown`.
- `never_spawns`: `vector<bool>`; true when pearl countdown negative.
- `visits_nopearl`: `vector<int>`; increments when `!ate` (no pearl eaten at visit).
- `last_visit_round`: `vector<int>`; round of last visit; updated when `has_previous_head && current_head != previous_head`.
- `eaten_here`: `vector<bool>`; true when `cell >= 0 && cell < cells && controller.get_tile(current_head)->has_pearl()`. Tracks eating event.
- `occupancy`: `vector<Occupancy>` length `cells`. `Occupancy` struct (`present`, `head`, `team`, `id`, `round`). Updated every turn: cleared (`occupancy[cell] = {}`), then if dragon part present set to `{true, is_head(), team, id, round}`.
- `pearl`: `vector<bool>`; `tile.has_pearl()` from vision.
- `own_body`: `vector<Vec2>`; collects all tiles where `part->get_id() == controller.get_id()`.
- `my_team`: `unswbc::Team`; set from `controller.get_team()` in `update()`; used by `get_cell()` for ally/enemy classification.
- `previous_head` / `has_previous_head` / `previous_move_ate`: Tracks movement state between turns for pearl-eaten logic (`previous_move_ate` set when `ate = true` at `current_head` after move).

---

## 2. Initialize (`memory.cpp` lines 7–28)

`Memory::initialize(map_width, map_height)` sets all vectors to `cells = width*height`. Initializes all bit arrays, pearl counts to `max()`, booleans to false, `own_body.clear()`, `previous_head = {}`, flags false. No heap after initialization (all `assign()` to fixed sizes).

---

## 3. Update Cycle (`memory.cpp` lines 30–98)

`Memory::update(const unswbc::Controller&, int round)` — main per-turn state sync.

Sequence:
1. Collect `visible_portals` from edges with `get_edge_type() == PORTAL`.
2. If `has_previous_head && current_head != previous_head`: compute `ate` (current tile has pearl); update `previous_move_ate`, `last_visit_round`, `visits_nopearl` / `eaten_here`.
3. Update `previous_head`, `has_previous_head`.
3a. `my_team = controller.get_team();`
4. Clear `own_body`; iterate `controller.get_tiles()`; for each tile:
   - `seen[cell] = true`; `last_seen_round[cell] = round`; `pearl[cell] = tile.has_pearl()`.
   - `countdown = tile.get_pearl_time()`; if `< 0` set `never_spawns = true`, `pearl_spawn_round = max()`; else `pearl_spawn_round = round + countdown`.
   - `occupancy[cell] = {}`; if dragon part present set fields; if `part->get_id() == controller.get_id()` push to `own_body`.
   - For each direction `d`: `known_edges[cell] |= 1<<d`; if `KELP` set `kelp[cell] |= 1<<d`; if `PORTAL` set `portal_id[cell][d] = portal_id`; if `PORTAL` also add to `visible_portals`.
5. Pair portals: for each endpoint, find other endpoint with same `id` but different `(cell, direction)`; set `portal_exit_known[endpoint.cell][endpoint.direction] = true`. This is the only way portal emergence is simulated (only when paired endpoint visible).

---

## 4. Queries (`src/core/memory/queries.cpp` / `.hpp`)

Implemented query functions (`get_cell`, `get_edge`) using `Memory` adapter (`my_team`, `seen`, `occupancy`, `kelp`, `portal_id`). Returns `pair<optional<int>, optional<int>>` per spec. `get_cell` uses `my_team` to classify ally/enemy (`3`/`4` vs `5`/`6`). `get_edge` uses direction mapping `0=N, 1=E, 2=S, 3=W`.

- `get_edge(memory, x, y, dir)`: `NULL` out-of-bounds; `-1` unknown edge; `0` empty; `1` kelp; `2` portal with `portal_id`.
- `get_cell(memory, x, y)`: first value `NULL` out-of-bounds, `-1` not visited, `0` empty, `1` own body, `2` own head, `3` ally, `4` ally head, `5` enemy, `6` enemy head; second value `NULL` out-of-bounds, `0` no pearl, `1` pearl.
- Existing `memory.cpp` helpers (`known`, `edge_known`, `is_frontier`) remain available.

---

## 5. Integration with BodyGuard / M2 / M3

- `Memory` provides `usable(x,y)` (seen + not obstacle) and `plain(x,y,d)` (not kelp, not portal, wrapped neighbor) via adapter logic in `map_view.hpp` / `torus.hpp`.
- `M2a` (`m2a.hpp`) uses `Grid` adapter (derived from `Memory` / `map_view`) for `buildRuns` (needs `usable` and `plain`). Current `Grid` placeholder returns `true` always; full integration requires real adapter from `Memory` state.
- `M3` validator (`validator.cpp`) uses `is_simple_cycle` on `Cycle` structs (now defined in `types.hpp`); `seals_pocket` uses `pocket` cell index from memory; `find_dash` requires `door` / `outside` cell selection (needs `Memory.position()` for torus adjacency).
- `sim.hpp` uses `Memory`-like state (but separate `World` with `Dragon`, `kelp_`, `pearl_`, `occupant_`) rather than calling `Memory::update()` directly; they are conceptually parallel (engine replay vs bot memory).

---

## 6. Unimplemented / Partial

- `Grid` adapter in `types.hpp` uses memory queries (`get_cell`/`get_edge`) for `usable`/`plain`. No `m2a.cpp` exists.
- No `map_store` persistence module separate from `Memory`; `sim.hpp` duplicates some fields (`occupant_`, `dragons_`).
- `initial_moves` / `previous_head` tracking is partial: only updates when `has_previous_head` true; no handling for first turn or split child initialization.
- `portal_exit_known` only pairs visible endpoints; does not handle hidden endpoints (matches `PROMPT.MD` rule: simulate portals only when paired endpoint visible).

---

## 7. File References

- `src/core/memory.hpp`: Struct definition, adapter interface (`usable` implied via external adapter)
- `src/core/memory.cpp`: `initialize()`, `update()`, `known()`, `edge_known()`, `is_frontier()`
- `src/bodyguard/core/types.hpp`: Added `Cycle`, `CycleCandidate`, `MAX_CYCLE_LEN`
- `src/bodyguard/core/torus.hpp`: `neighbor()`, `neighbor_cell()` used by `Memory.index()` / `Memory.position()` and by `map_view.hpp`
- `src/bodyguard/core/map_view.hpp`: Vision window adapter (`usable`, `plain`, `step`, `seen`)
- `tests/bodyguard_sim/sim.hpp`: Engine replay state (parallel to `Memory` but independent)
- `tests/bodyguard_sim/seed46_discrepancy.md`: Replay/function divergence documentation
- `docs/PROMPT.MD`: Behavioral rules (vision 7×7, sonar 64-bit, split/valve, death rules, pearl countdown)
- `docs/MemoryImplementation.md`: Section "Planned Queries" updated to reflect implemented `get_edge`/`get_cell` in `src/core/memory/queries.cpp` using `optional<int>` returns, Memory adapter source (`my_team`, `seen`, `occupancy`, `kelp`, `portal_id`), direction mapping `0=N/1=E/2=S/3=W`, and team-based ally/enemy classification (3/4 ally, 5/6 enemy) via `memory.my_team`.
- `docs/MemoryImplementation.md`: Added `my_team` field reference and `controller.get_team()` initialization in update cycle.
- `docs/MemoryImplementation.md`: Added query return semantics table (`NULL` out-of-bounds, `-1` unknown, `0` empty, `1` kelp, `2` portal with `portal_id`; `get_cell` first value `-1` not visited / `1` own body / `2` own head / `3` ally / `4` ally head / `5` enemy / `6` enemy head; second value `0` no pearl / `1` pearl).
- `docs/MemoryImplementation.md`: Added file reference `src/core/memory/queries.md` (planned module spec).
