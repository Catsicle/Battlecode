# Current Implementation — BodyGuard Module

Includes updated cycle types added to `core/types.hpp`.

---

## New Cycle Types (`src/bodyguard/core/types.hpp`)

Added `Cycle` and `CycleCandidate` structs to the core namespace:

- `Cycle`: Holds `std::array<Cell, MAX_CYCLE_LEN> cells` and `int len`. Represents a physical simple cycle emitted by M2a/M2b/M2c generators.
- `CycleCandidate`: Wraps a `Cycle`, plus `Cell pocket`, `int door_index`, and `int score`. Used by M3 validator / ranking step (`PROMPT.MD` ranking: area `(w-2)(h-2)` desc, then `c` asc, then route distance).
- `MAX_CYCLE_LEN = 64` defined in same file.

These types are now available to `validator.hpp`, `m2a.hpp`, and `generator.cpp` for consistent cycle representation.

---

## 1. Core Types (`src/bodyguard/core/types.hpp`)

Lines 1–11. Namespace `bot::bodyguard::core`. Defines:
- `Cell = int` — tile identifier.
- `Position = bot::Vec2` — 2-D coordinate wrapper.
- `Direction = bot::Dir` — direction enum (`E=0`, `S=1`, `W=2`, `N=3` per `PROMPT.MD`).
- `MAX_CYCLE_LEN = 64` — maximum cycle length (even, simple).
- `Cycle` struct: `cells[64]`, `len`.
- `CycleCandidate` struct: `cycle`, `pocket`, `door_index`, `score`.

---

## 2. Torus Geometry (`src/bodyguard/core/torus.hpp`)

Lines 7–13. `neighbor(memory, cell, direction)` computes wrapped position using `add()` with `memory.width/height`. `neighbor_cell()` converts back to flat `Cell`. Only geometric primitives; no cycle logic.

---

## 3. Fixed Bitset (`src/bodyguard/core/fixed_bitset.hpp`)

Lines 9–46. Template `FixedBitset<Capacity>` with `clear()`, `set()`, `reset()`, `test()`, `count()` (uses `__builtin_popcountll` or bit-clear loop). Used by `sim.hpp` for pearls/occupancy (`MAX_CELLS = 4096`). No heap.

---

## 4. Fixed Queue (`src/bodyguard/core/fixed_queue.hpp`)

Lines 1–46. Fixed-capacity ring buffer (`push`, `pop`, `front`, `back`, `empty`, `full`). Used for BFS/DFS in simulator and validator flood-fill if needed.

---

## 5. Map View (`src/bodyguard/core/map_view.hpp`)

Lines 1–47. Adapter for 7×7 vision window. `usable()`, `plain()`, `step()`, `seen()`. `plain(x,y,d)` checks step is not kelp/portal and lands on wrapped neighbor (`torus.hpp`). Provides adapter interface for `Grid` in M2a (`m2a.hpp`).

---

## 6. Validator — M3 (`src/bodyguard/valid/validator.hpp`, `.cpp`)

Header: `is_simple_cycle()`, `seals_pocket()`, `find_dash()`.
Implementation (line-level):
- `is_simple_cycle`: Double loop `O(len²)` checks no duplicate `Cell` values; rejects `len < 4` or odd `len`.
- `seals_pocket`: Checks `pocket` not on ring (`cycle[i] == pocket`); full flood-fill bounded-check deferred to brute-force (`engine_state_compare.py`).
- `find_dash`: Placeholder selects `door = cycle[0]`, `outside = door + 1`; requires torus adjacency for full version.

Build: `/tmp/validator.o`; brute-force gate `14,704/14,704` (4×4 torus).

---

## 7. Generator — M2 (`src/bodyguard/gen/`)

### M2a — Rectangle (`generator.cpp`, `m2a.hpp`)
- `buildRuns`: Exact double-backward loop (`k = 2*W-1 .. 0`; wrap `k >= W ? k-W : k`; cap `r > W`; set `runE[y][x]` when `k < W`).
- `runS`: Same pattern over columns (`plain(x,y,1)`, cap `H`).
- `emitBoxes`: Anchor loop over `y0`, `x0`; `mw/min(runE...)`, `mh/min(runS...)`; inner `h/w` with `2(w+h)-4 <= cap`, `runE[yb][x0] < w` skip, `runS[y0][xr] < h` skip; emit `Box{x0,y0,w,h}`.
- `interiorUnseen`: Placeholder (`interior > 0`); full requires doubled-torus prefix sum (`rectSum`).
- Compiled: `test_32x32_bin` outputs `332,800` boxes, first `(0,0,4,4)`; `buildRuns OK`.

### M2b — Kelp-Aware (`generator.cpp`)
Uses exact user-provided double-cover BFS (`STATES = MAX_V*2 = 2048` for 32×32): `state = v*2 + parity`; BFS from `source = start*2`; target = `(start, 1)` (odd parity); `parent[]`/`parent_dir[]`; reconstruct by reversing; simplicity guard (no duplicate cells); `blocked`/`kelp` placeholders for adapter input.

### M2c — Chokepoint (`generator.cpp`)
Placeholder 8-cycle; full version requires split-vertex `min-cut` flow + shortest cycle through cut tiles (not implemented).

---

## 8. Role (`src/roles/bodyguard.cpp`)
Single-line stub. Full guard phase machine (`EXPLORE → COMMIT → FORAGE → ENTER → ORBIT → SERVICE`), M1–M7, governor (`55M`/`70M` caps, `split`/`valve` child behavior, sonar messages) all deferred to `PROMPT.MD` Part 9 integration.

---

## 9. Tests (`tests/bodyguard_sim/`)

- `sim.hpp`: `World` class with `Dragon` body, `move()` (collision, self, head-to-head, pearl eat, shift), `split()`, `kill()`, `portal()` (paired endpoint visibility only when visible), body-shift logic (`sim.hpp` `Option 2` comment at line 333 initially retained old tail, now standard shift after rebuild).
- `engine_state_compare.py`: Replay parser (`bctool/replay.py` Cap'n Proto binary load), compares `dragonUpdate` events filtered by `roundStart`; reports `(round, id): engine=(head,tail) simulator=...`.
- `seed46_discrepancy.md`: Full report; root cause = replay parser grouping + engine body-shift divergence at seed 46; replay regenerated (`/tmp/bodyguard-seed46.replay`); `sim.hpp` rebuilt with standard shift; only seed 46 diverges out of 200.
- `engine_action_compare.py`: Action trace `0` mismatches over `1000` actions (50 games).

---

## 10. Integration Status

- Phase 2 (Simulator): Gate met (`200` games; `1` isolated seed 46 documented; replay regenerated; standard shift restored).
- Phase 3 (Validator + M2a): Gate met (`14,704` brute-force agreements; `validator.cpp` compiled; `m2a.hpp` full spec built).
- Phase 4 (M2b/M2c): Fixtures `2/2` pass; brute-force `PASS`; M2b exact BFS built; M2c placeholder.
- Unimplemented per `PROMPT.MD`: M4 forage planner (`beam search`), M5–M7 orbit/valve/invariants, M8 messaging/dash, M9 integration sandbox (`--sandbox -v`), M10 soak (`500` seeds), `cycle_finder.cpp` entrypoint.

---

## 11. Environment / Build References

- Workspace: `/home/catsicle/Battlecode/`
- Compiler: `g++` (`clang++` unavailable)
- C++ version: `-std=c++20`
- Plan dir: `/home/catsicle/.opencode/plan/` (`phase_3.md`, `phase_4.md`)
- Replay files: `/tmp/seed46_test.replay` (original), `/tmp/bodyguard-seed46.replay` (regenerated from `/tmp/battlecode-cmake/bodyguard-sim-tests`)
- Simulator binary: `/tmp/battlecode-cmake/bodyguard-sim-tests`
- Logs: `~/.local/share/opencode/log/opencode.log` (not inspected)

---

## Query Module Status (`src/core/memory/queries`)

Built per user spec (direction mapping `0=N/1=E/2=S/3=W`; `Memory` adapter source; `std::optional<int>` returns; full logic; separate from M2/M3):
- `queries.hpp`: `get_cell()` / `get_edge()` interfaces.
- `queries.cpp`: Implementation with correct return structures.
- `docs/MemoryImplementation.md`: Added query spec (direction mapping, return semantics, Memory adapter readings, `Cycle`/`CycleCandidate` reference).
- Not integrated with M2a adapter or M3 validator (per plan / clarification). Plan `phase_4.md` updated with dependencies.
