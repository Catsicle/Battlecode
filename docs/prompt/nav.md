# TASK: Complete the navigation safety stack in the Battlecode AU bot (C++)

Workspace layout (already exists): `src/main.cpp`, `src/config.hpp`, `src/core/{memory,types,log}`, `src/nav/{astar,bfs,flee,grid,wander}`, `src/roles/{role,registry,stub,explorer,killer,feeder,bodyguard,queen}`, `src/comms/{comms,msg}`.
Sonar payload decoding is OUT OF SCOPE: leave `msg.hpp::decode_navigation_target` as is. The hardcoded target (`NAVIGATION_FIXTURE`, `FIXTURE_TARGET_X/Y` in config.hpp) stays the only Navigate-mode target source. Only the stub role is real; the other role files stay empty placeholders. Do NOT edit helper.hpp. Ask before touching anything not listed in section 2.

## 0. Rules of engagement
1. Before coding, read helper.hpp, bot.toml, role.hpp, registry.cpp and the docs at https://game.battlecode.au/docs/<page>: overview, map-info, pearls, kelp-and-portals, vision, movement, splitting, sonar, death, execution-order, timeouts. Docs win over this prompt. Record every conflict in docs/NOTES.md.
2. Anything marked `VERIFY` is an assumption. Implement it behind one named function/constant, add a `// VERIFY` comment, and resolve it from docs/helper.hpp. List the resolution in docs/NOTES.md.
3. Work in the phases of section 2. After each phase: build, run all tests, update the docs, and STOP and report if anything fails. Never start a phase on a red build.
4. Check how bot.toml compiles sources and register every new .cpp.
5. CPU: worst case <= 15M points per turn on a 64x64 map (limit is 100M). No iostream/stdout in hot paths or release builds (each write costs 2.5M points). No heap allocation inside the per-turn search loops: use persistent workspaces with generation stamps instead of `std::vector` per call.
6. One transition function, shared by planner and validator (section 3.1). Never duplicate move rules.

## 1. Pre-existing defects to fix first (Phase A)
A1. Ordered own body. `Memory::own_body` is built by iterating visible tiles: it has no segment order and misses segments outside the 7x7 window. First check helper.hpp for an ordered body or a segment-index accessor; if one exists, use it. If not, create `src/core/body_tracker.{hpp,cpp}`:
 - Keep `std::deque<int> body` (head first) across turns.
 - After a validated move prefix is emitted, set the predicted body from the Sim result (section 3.2).
 - Next turn, reconcile: head tile == predicted head, `get_length()` == predicted length, and the set of own segments visible in vision equals the predicted body restricted to vision.
 - On mismatch (unexpected pearl, spawned pearl, split, damage), rebuild by walking from the head over adjacent (incl. portal-linked) own-segment tiles, resolving ambiguity with the prediction, and trust the prediction (truncated to `get_length()`) beyond vision.
 - Reset the tracker after a split. Count mismatches in a stats struct.
A2. Pearl/visit bookkeeping. `Memory::update` tests `has_pearl()` on the head tile at the START of the next turn, which is after the pearl was eaten, so `eaten_here` is probably never set (VERIFY by reading helper.hpp/docs). It also only records the final head tile. Replace with a pure function `Memory::apply_move_record(const MoveRecord&, int length_now, int round)` where `MoveRecord` = traversed cells and `pearl_known_before[]` captured at planning time. Rules: `eaten = length_now - length_before`; mark the first `eaten` pearl-flagged traversed tiles as `eaten_here=true`, `pearl=false`; every other traversed tile gets `visits_nopearl++`; every traversed tile gets `last_visit_round = round`.
A3. Occupancy staleness. `Memory::occupancy` is trusted forever after leaving vision. Extend `Occupancy` with `segment_index` and `owner_length` (from helper if available, otherwise -1 = unknown). Add `bool Memory::occ_fresh(cell, now)` = `now - occ.round <= OCC_STALE_ROUNDS` (new config, default 3). Never treat own segments as occupancy; own body comes from the BodyTracker.
A4. `approach_target` and `wander` run a full Dijkstra per candidate. Replace with ONE search from the head that fills a distance field (section 3.4), then pick the destination from the field.
A5. `wander` must score `S(v) = G(v) - D(v)` using the field (section 3.5). Currently it ignores path cost and uses `last_seen_round` instead of `last_visit_round`.
A6. Roles must stop returning finished paths. Extend `Action` with `nav::PlanRequest { NavMode mode; bool has_target; Vec2 target; }`. The stub fills it (Navigate toward the fixture/inbox target, else Explore). `main.cpp` calls `nav::execute_turn(...)` (section 3.7). Keep `Action::path` marked deprecated until the stub is migrated, then remove it.
A7. Fix the config.hpp comment: NAVIGATION_FIXTURE is `true` but the comment says disabled.

## 2. Phases and files
- Phase A: section 1 + tests T0-T1 (memory bookkeeping, body tracker).
- Phase B: `src/nav/transition.{hpp,cpp}` (3.1), `src/nav/sim.{hpp,cpp}` (3.2).
- Phase C: `src/nav/safety.{hpp,cpp}` (L1, L4, L3: 3.3), tests T2-T9.
- Phase D: `src/nav/astar.cpp` refactor to field search with time-indexed own-body state (3.4), `src/nav/planner.{hpp,cpp}` (3.5, 3.6).
- Phase E: `src/nav/executor.{hpp,cpp}` (3.7) + main.cpp wiring, replace `flee.cpp`/`bfs.cpp` uses.
- Phase F: `src/nav/dial.hpp` behind `USE_DIAL` (3.8).
- Phase G: docs, scripts, final report (section 5).
All new constants go in config.hpp: `C_MAX=192`, `TRAP_EXTRA=2`, `MAX_RETRY=4`, `OCC_STALE_ROUNDS=3`, `DEFAULT_ENEMY_REACH=2` (VERIFY), `USE_DIAL=false`, `NAV_STATS=false`.

## 3. Specifications

### 3.1 Transition function (transition.hpp)
```cpp
struct Step { int dest; uint8_t new_dir; enum Kind { Normal, PortalOK, PortalUnresolved, Kelp } kind; };
Step resolve_step(const Memory& m, int tile, int dir);
```
- Kelp edge on (tile,dir): `Kelp`. No portal on the edge: `Normal` with the wrapped neighbour.
- Portal edge: pair endpoints by portal id. Add `PortalEnds{int tile[2]; uint8_t dir[2]; uint8_t n;}` indexed by id, filled when `Memory::update` records a portal edge (this replaces the unused `portal_exit_known`). Pair resolved iff `n==2`; otherwise `PortalUnresolved`.
- Exit tile and exit direction live in ONE function `portal_exit(...)`, `// VERIFY` against docs/kelp-and-portals. Also VERIFY whether the exit side can be kelp; if the exit edge is unobserved return `PortalUnresolved`, if known kelp return `Kelp`.
- Navigate mode forbids `PortalUnresolved`; Explore mode allows it at cost `C_PORTAL_UNKNOWN`.

### 3.2 Sim (sim.hpp): reusable dragon body, O(1) apply/undo
```cpp
struct Undo { int tile; int old_stamp; bool took_pearl; int old_head; };
struct Sim {
  int L, counter, head, e;          // e = pearls eaten in this simulation
  std::vector<int> stamp;           // stamp[tile] = counter when entered; very negative if never
  std::vector<uint8_t> pearl_taken; // pearls consumed inside the simulation
  void reset(const std::deque<int>& ordered_body);   // stamp[B[j]] = -j, counter = 0
  int  seg_index(int tile) const;   // counter - stamp[tile]; occupied by self iff stamp valid && < L
  Fail try_step(const Memory&, const Occupants&, int dir, int rho, Undo&); // L1 rules; applies on success
  void undo(const Undo&);
};
```
Applying a step: `stamp[dest] = ++counter`; if `memory.pearl[dest] && !pearl_taken[dest]` then `L++`, `e++`, `pearl_taken[dest]=1` (tail does not advance, so no vacate); otherwise the tail vacates automatically because its age reaches `L`. `undo` restores everything. Fuzz-test apply/undo.

### 3.3 Safety layers (safety.hpp)
`enum Fail { OK, KELP, SELF, OTHER_BODY, HEAD_ON, PORTAL_UNRESOLVED, TRAP, DEAD_END }`.

L1 `try_step` order, using `resolve_step` only:
 1. `Kelp` or `PortalUnresolved`: fail.
 2. Own body: occupied iff `seg_index(dest) < L` (PRE-move occupancy; the tail counts as occupied on that step, because the game kills you even if the tail would vacate). Fail SELF.
 3. Other dragons, only if `memory.occ_fresh`: if `segment_index` and `owner_length` are known, blocked iff `rho + age < owner_length - segment_index`; if unknown, blocked for `rho == 0`. Fail OTHER_BODY.
 4. Head-on: any other head with torus/graph distance `<= s_other` (or `DEFAULT_ENEMY_REACH` if unknown) at `rho == 0`. Fail HEAD_ON.
 5. Apply.
`bool replay(Sim&, const Path&, int x, Fail&)` applies the first x steps; `rho(t) = (t-1) / s` with `s = ceil(L0/4)`, `t` = step number starting at 1.

L2 closed form for the planner (identical to the stamp rule): `own_blocked(tile,t,e) = j0[tile] >= 0 && j0[tile] <= L + e - t`.

L4 `trap_ok(const Sim& after, ...)`: BFS from the head over known tiles through `resolve_step`; depth d is 1-based; a neighbour is passable iff it is not `Kelp/PortalUnresolved`, not blocked by `after`'s own body using the time rule (`seg_index < L - (d-1)`... implement as `seg_index(v) + (d-1) < L`), and not held by a fresh other dragon. Accept iff reached tiles `>= L + e + TRAP_EXTRA` (early exit) OR an unseen tile/frontier is reached.

L3 `bool surv(Sim&, int k)`: `k==0` true; for each of 4 dirs: `try_step`; if OK, recurse and undo; return true on first success. If the destination tile is UNSEEN, return true (optimistic: it cannot be judged; without this every frontier move would fail). Depth `LOOKAHEAD_K`.

`bool validate_prefix(Sim s0, const Path&, int x, Fail&)` = replay, then `trap_ok`, then `surv`.

### 3.4 Search: one field per planning call (astar.cpp refactor)
```cpp
struct SearchField { /* views into a persistent workspace */ int dist(int tile) const; int steps(int tile) const; Path extract(int dest) const; };
SearchField build_field(const Grid&, const Sim&, NavMode, uint8_t forbid_first_mask, int target_or_minus1);
```
- Costs (x4 fixed point): `step = C_STEP + (portal ? C_PORTAL_EXTRA : 0) + (unseen ? C_UNKNOWN : 0) [Navigate only] + pen(v) [Explore only] + (PortalUnresolved ? C_PORTAL_UNKNOWN : 0) [Explore only]`.
- `pen(v) = 0 if eaten_here[v] else min(P_MAX, LAMBDA*visits_nopearl[v]*AGE_K/(AGE_K+age_v))`, `age_v = round - last_visit_round[v]`; if `EXEMPT_SPAWNABLE` also 0 for tiles with `pearl_spawn_round != INT_MAX`. Keep this exactly as in the existing code but without the `navigate ? C_STEP` override hack.
- Navigate treats unseen tiles as passable at `C_UNKNOWN` (optimistic). The committed prefix is truncated at the first unseen tile (section 3.7). Remove the per-frontier search loop from `approach_target`.
- State key: tile for non-body tiles; for tiles with `j0 >= 0` (own body) the key is `(tile, min(t, HORIZON_T))` via a slot table, with `own_blocked(tile,t,e)` as a hard block. Labels carry `t` and `e` (pearls on the path from `memory.pearl`). `forbid_first_mask` forbids those first directions from the start tile. Replay (L1) remains the final authority, so rare e-mismatches just become retries.
- Stop early only when a single target was requested and popped; otherwise fill the whole field.
- Workspace: persistent arrays + generation stamp, no allocation per call.

### 3.5 Planner (planner.hpp)
`Path plan(const PlanRequest&, const Grid&, const Sim&, uint8_t forbid_mask)`.
- Navigate: `build_field` with the target; extract the path. If the target is unreachable even optimistically: return not found.
- Explore: build the full field, then `S(v) = G(v) - D(v)` for every reachable KNOWN tile, with `G(v) = B_PEARL*[pearl(v)] + B_SOON*0.7*[no pearl now && pearl_spawn_round[v] <= round + rho(steps(v)) + 1 && !never_spawns[v]] + B_FRONT*[frontier(v)] + B_STALE*min(age_v,200)/200`. Choose argmax S; if `max S <= 0`, choose the reachable frontier tile with least D, else the tile with greatest `age_v`. Tie-break: more pearls on the path, then fewer steps.

### 3.6 Multi-step emission rules
Emit `x <= s = ceil(L/4)` moves per turn (free steps only; paid sprint is never used outside the emergency of 3.7). Truncate the prefix at the first step whose destination is unseen (Navigate). Prefer routes with fewer turns, `ceil(n/s)`, then lower cost.

### 3.7 Executor and retry controller (executor.hpp)
`void execute_turn(unswbc::Controller&, Memory&, BodyTracker&, const Grid&, const PlanRequest&)`:
```
forbid = 0
for attempt in 0..MAX_RETRY-1:
    path = plan(req, grid, sim0, forbid);  if !found: break
    for x = min(s, path.length) down to 1:         // shorten the prefix FIRST
        Sim s = sim0 (reset); Fail f
        if validate_prefix(s, path, x, f): emit make_moves(steps[0..x)); record MoveRecord + predicted body; return
        stats[f]++
    forbid |= 1 << dir_index(path.steps[0])         // then blacklist the failed first edge
fallback 1: safest_single_move = argmax over dirs that pass try_step by (surv depth reached, then trap area, then tile cost)
fallback 2: split if try_step fails for all dirs: keep `can_split(length-2)/do_split(length-2)`, but VERIFY semantics in docs/splitting; only when NO move passes L1; reset BodyTracker after a split
fallback 3: make_move(controller.get_dir())
```
`safest_move` in flee.cpp and `reachable_area` in bfs.cpp must be rewritten to use `try_step`/`trap_ok`, or deleted if unused. Remove `emit_count = std::min({..., 1})` from main.cpp.

### 3.8 Dial queue (dial.hpp, last)
Intrusive bucket lists: `head[C_MAX+1]`, pools `next[]/node_of[]`, bucket index `cost % (C_MAX+1)`, lazy deletion (`dist[node] != d`), live-entry counter for termination. Requires every edge cost <= `C_MAX`: clamp penalties. Behind `USE_DIAL`; the heap version stays as default until Dial is proven equal by test T9 AND measured faster.

## 4. Tests (create `tests/`, plain C++ with a tiny CHECK macro, no external deps; one script `tests/run_tests.*` that works on Windows; document the exact compiler command)
Test helper: build a `Memory` from an ASCII map without a controller. Edge-grid format: cells at odd coordinates `(2x+1,2y+1)`, edge chars at even coordinates (`' '` open, `'#'` kelp, `'0'-'9'`/`'a'-'z'` portal id). Decide and document how the wrap edges are written (both borders must agree, assert it). Check that helper.hpp can be included in a plain test binary without the engine runtime.

Independent oracle: `tests/oracle.cpp` re-implements the game rules from the docs separately from `Sim::try_step`, step by step. Use it for differential testing.

- T0 Memory: `apply_move_record` for multi-step moves with and without pearls; stale occupancy ignored after OCC_STALE_ROUNDS.
- T1 BodyTracker: predicted == observed (no change), pearl growth mismatch, split reset, chain-walk rebuild, body longer than vision.
- T2 `resolve_step`: normal, wrap, kelp, resolved portal, unresolved portal, exit-into-kelp (per the VERIFY resolution).
- T3 Sim apply/undo: 1000 random walks of apply+undo must restore stamps, L, e, head exactly.
- T4 Hairpin (L=4, body `[(5,5),(4,5),(3,5),(2,5)]`, head (5,5), no pearls): `S,W,N` rejected with SELF at step 3; `S,W,W,N` accepted (ends at (3,5)).
- T5 Pearl growth (L=3, body `[(5,5),(4,5),(3,5)]`): `S,W,N` accepted with no pearl on (5,6); with a pearl on (5,6) it is rejected at step 3 (formula: `j0=1 <= L+e-t = 3+1-3`).
- T6 Others: fresh enemy body on the path rejected at rho 0; the same occupancy with age 10 ignored; enemy head within reach rejected.
- T7 Differential: 5000 random (map, body, path) cases, `replay` result == oracle result.
- T8 Search property: for random maps/bodies, every path returned by `plan` passes `replay` for its first `min(s, n)` steps (no SELF failure may come from the search). Time-indexed key: a map where the shortest static route crosses the own tail zone and only works at later arrival.
- T9 L4: 3x3 room with a one-tile door: entering fails for L=8, passes for L=3; frontier-reaching region passes. L3: 2-deep dead end, L=5 fails; an unseen next tile returns true. Dial vs heap: identical distance fields on 200 random graphs.
- T10 Retry controller: map where a 3-step prefix fails and a 1-step prefix passes (emitted prefix is 1); map where the cheapest route's first edge always fails (emitted move comes from the second-best route); map with no safe move (split path chosen only then).
- T11 Planner: Explore picks `argmax S` (construct a case where a nearer small reward beats a farther large one); Navigate through unseen tiles truncates the prefix at the first unseen tile.
- T12 Mini game loop: with the oracle as the "game", run the full `execute_turn` loop on a spiral corridor map (long dragon, L >= 12) and a kelp maze for 500 turns: zero deaths.
- Integration: `scripts/nav_smoke.py` runs `unswbc run --sandbox -v` on 3+ maps using the fixture target. Inspect `unswbc run --help` and the replay format first; report whether the target was reached, rounds taken, any death round, and max points per turn. Acceptance: max points per turn <= 15M on the largest map.

## 5. Documentation (mandatory; the reader is a human who did not watch you work)
Create:
- `docs/NAVIGATION.md`: architecture overview with an ASCII diagram of the turn pipeline (memory update -> role -> PlanRequest -> planner -> field search -> validator L1-L4 -> retry controller -> emit); a glossary (L1, L2, L3, L4, Hard, Soft, rho, s, e, stamp, field, frontier); the rationale for each design decision; the exact formulas; worked examples (the hairpin and pearl-growth cases from T4/T5).
- `docs/CHANGELOG-navigation.md`: every file created/modified/deleted, one entry each: what, why, which requirement it satisfies.
- `docs/NOTES.md`: every `// VERIFY` item with its resolution, every doc conflict, every deviation from this prompt and why.
- `docs/TESTING.md`: how to run each test and script, expected output, what each test protects against, how to add a new scenario.
- `docs/TUNING.md`: every config constant, what it does, safe ranges, and which stats counter tells you which to tune.
- Update `nav.md`: turn the status table into the true current status (Implemented / Partial / Not implemented) with file and function references.
- Every new file: a header comment (purpose, invariants, who calls it). Every public function: a doc comment with inputs, outputs, complexity, and failure modes. Comment non-obvious lines (the stamp trick, the time-indexed state key) with the reason, not just the what.
- Final report in the chat: the list of files changed, test results (pass/fail counts), measured max points per turn, unresolved VERIFY items, and known limitations (e.g. K=3 only catches dead ends <= 2 deep; frontier acceptance in L4 is optimistic; enemy motion is only approximated).