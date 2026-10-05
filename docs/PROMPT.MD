# Build plan: ring-forming guard dragon (C++)

## 1. Problem statement

**World.** A torus of W×H tiles (W, H ≤ 64, to be confirmed in Part 0). Each tile edge is empty, kelp or portal. Tiles hold a pearl flag and a countdown τ. Dragons are bodies of distinct tiles.

**Roles.** The queen Q (lowest id), the guard G (explorer, then orbiter), relays, and valve children.

**Given.** G starts at length 2, remembers state between turns, and sees only the 7×7 window around its head. Each dragon has 100M points per turn.

**Find.** A policy for G and a simple cycle C of even length c such that, by round T:

1. G is alive, and its body is c−1 consecutive tiles of C.
2. The queen's target pocket P is separated from the exterior in the graph with kelp edges removed and C's tiles deleted. The single free tile of C is tolerated. P must contain a cycle of length ≥ ℓ_end + 1 (ℓ_end is the queen's length after entering).
3. The queen has a collision-free route to a waiting tile D beside the door tile X ∈ C. Her dash (s steps) clears X within one turn, while X is the free tile.
4. No dragon of yours dies, and no turn exceeds the point budget.

**Output.** FOUND(C, route, dash path), NONE_IN_KNOWN, or NONE_IN_BUDGET.

**Note on kelp.** Because the guard must orbit a closed cycle, kelp can't shorten a ring around open space. Its use is to let a small cycle plug a kelp-bounded corridor, so the barrier is C's tiles plus kelp. Sealing is therefore tested by flood fill, not by a polygon-interior argument.

## 2. In-game context

| Rule | Status |
|---|---|
| 100M points per dragon per turn, 48 MB, overrun means death | Verified (Timeouts) |
| Price list: simple ops 1, loads/stores/branches 2, call 4, division 3; C++ compiled with `-O2 -msimd128`, no other libraries | Verified |
| One stdout flush per turn at 2.5M + 4,000 per byte; no `sync_with_stdio(false)`, no `unitbuf` | Verified |
| Dragons act in ascending id order; pearls tick first | Verified (Execution Order) |
| A split child acts later in the same round | Verified |
| Default action is suicide | Verified |
| First ceil(L/4) steps per turn are free; each further step removes one extra tail segment; a length-2 dragon can't pay | Verified |
| The self-collision check includes the tail, before it advances | Verified |
| Death drops pearls on every second segment, starting at the head | Verified |
| Sonar: 64-bit messages, up to four per turn, straight line, first body or kelp stops it | Verified |
| Vision is 7×7 | From you; confirm in Part 0 |
| Coordinates absolute and shared; kelp representation; cooldown display offset; child orientation; helper API; map size range | **To probe in Part 0** |

## 3. Algorithm

**Per-turn loop** for every explorer dragon. The same program runs everywhere, so it dispatches on role:

1. Read the round, then classify: valve child (length 2, an ally head directly ahead, no memory), queen, relay, or guard.
2. Valve child: print `ENDTURN` with no action (default is suicide).
3. Guard: update the map store, process messages, run the governor, advance the phase machine.

**Guard phase machine:** EXPLORE → COMMIT → FORAGE → ENTER → ORBIT (with VALVE) → SERVICE (messages). It stays in ORBIT until the end.

**Modules:**

- **M1 Map store.** One record per tile (about 8 bytes, 32 KB at 64×64). It holds the seen flag, edge types, pearl flag, τ and the last-seen round. Coordinates are wrapped.
- **M2 Candidate generators** (all produce simple cycles with c ≤ c_cap):
  - (a) Rectangle rings by prefix sums, anchored around a free pocket.
  - (b) Kelp-aware cycles around a room: a shortest odd-crossing closed walk on a double cover, within a local ≤ 32×32 window. Keep only the simple results.
  - (c) Chokepoint plugs: a minimum vertex cut between the pocket and the far exterior (flow on a split-vertex graph), then the shortest simple cycle through the cut tiles.
- **M3 Validator.**
  1. Check that the cycle is simple and has no kelp or portal on a ring edge.
  2. Delete C, remove kelp edges, and flood fill from P. The component must stay bounded.
  3. Find a cycle of length ≥ ℓ_end + 1 in the component.
  4. Pick the door X ∈ C, a free outside tile D, and a self-avoiding dash path of s steps.
- **M4 Forage and lap planner.** Count pearls needed (c−3 minus the head start), the pearls already present, and guaranteed first spawns from visible τ. Find a route by beam search. Enter the ring on a tangent so the approach trail doesn't overlap ring tiles ahead.
- **M5 Explorer.** Choose the frontier by the value of information. Use an optimistic bound (unseen tiles treated as free) to decide when to commit. Only take steps that leave an escape route in the known graph.
- **M6 Orbit and valve.** Keep L = c−1 on the cycle. If the guard reaches L = c, split off a length-2 child on the next turn, which dies and drops one pearl the parent then eats.
- **M7 Governor.** Meter points through the in-sandbox clock. Soft cap 55M, hard cap 70M. Heavy work is an anytime task that checks the meter every ~10k operations.

## 4. Implementation

The main bot will have multiple modes, one of those modes is explorer bot. The explorer bot on every turn will use the cyclefinding code, make the bot's main.cpp use the cycle_finder.cpp
- **Layout:** Inside 'src/' then 'bodyguard/' then `core/` (types, torus, bit sets), `map/`, `gen/` (M3), `valid/`, `plan/`, `ctl/`, `msg/`, `role/`, `sim/` (mini engine), `tests/`, and a single `cycle_finder.cpp`.
- **Rules:** fixed-size arrays and no allocation after init; no iostream in hot paths; `std::clog` only for debug, behind a compile flag; one flush per turn; deterministic seeds.
- **Mini simulator:** it implements the verified rules above, so most testing runs offline. Anything it assumes is validated against the real engine in Part 2.

## 5. Testing, in gated parts

Each part has to pass its gate before the next starts.

| Part | Build | Tests | Gate |
|---|---|---|---|
| 0 | Recon bots | Probes for each "to probe" item; print results | Rules ledger fully verified |
| 1 | Core + map store | Torus wrap, update-from-window, round-trip, symmetry on synthetic maps | 100% unit pass |
| 2 | Mini simulator | Replay scripted bots in both the simulator and the engine; compare positions, lengths, deaths | Trace match on ≥ 50 scripts |
| 3 | Rectangle generator + validator | Brute force all simple cycles on ≤ 6×6 tori and compare with the validator | Zero disagreements |
| 4 | Kelp generators (b, c) | Corridor-plug maps; min-cut vs brute force; flood-fill sealing vs ground truth | All plugs found on hand-made maps |
| 5 | Forage and lap planner | Scripted pearl layouts; assert no self-collision and correct final length | 100% on fixtures |
| 6 | Explorer + safe movement | 1,000 random maps in the simulator; assert no deaths and measure coverage | 0 deaths |
| 7 | Orbit + valve | Pearl-storm sequences; invariants: never death, L ≤ c−1 after the valve, the pearl count matches | No violations in 10⁶ steps |
| 8 | Messaging + queen dash | Pack/unpack fuzz; tag forgery rate ≈ 1/1,024; exhaustive dash timing over L_q ∈ 2..7 and every gap phase | The dash never kills the guard |
| 9 | Integration | Full runs on contest maps in the engine with `--sandbox -v`; log points per turn | p99 ≤ 55M, max ≤ 70M; success rate ≥ 95% by T |
| 10 | Soak | 500 seeds × all maps | No deaths, no overruns, regression table frozen |

**Standing invariants** (checked in every test): never two dragons on a tile, never a death on the ring, L ≤ c−1 on the ring after any valve, and the governor never exceeds its hard cap.

