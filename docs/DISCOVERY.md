# Discovery

Date: 2026-10-05

## Environment

- The active branch is `adarsh`.
- The active bot is `src/`, and the runner accepts bot directory paths directly.
- `uv run unswbc` resolves `unswbc 1.2.9` from the project environment. The
	standalone PATH executable reports `1.2.2` and prompts about `1.2.3`; Part 0
	game probes used `uv run unswbc` consistently.
- The local machine has GCC 11.4 and CMake. The judge documentation specifies
	Clang 20, C++20, `-O2`, and `-msimd128`.
- `src/bot.toml` lists multiple C++ translation units. The CMake target and the
	real runner both build the modular source layout successfully.
- The runner's default map/bot interface is:
	`unswbc run <map> <bot-directory> <bot-directory> [options]`.
	No staging workaround is required for this workspace.

## Official rules verified

- Maps are rectangular, with width and height in `[10, 64]`, and coordinates
	start at `(0, 0)` in the top-left. Map edges wrap toroidally.
- Vision is a wrapped 7x7 window, with Chebyshev radius 3. Vision does not see
	through portals.
- Kelp and portals are edge properties. Kelp kills on crossing. A portal pair
	shares an ID and orientation; crossing emerges from the partner edge.
- Pearl countdowns are rounds until the next spawn attempt. `-1` means the tile
	never spawns. Countdown processing occurs before dragon turns.
- Dragons act in ascending ID order. A child created by splitting acts later in
	the same round and receives a fresh process/memory state.
- The first `ceil(length / 4)` sprint steps are free. Further steps consume
	body segments, and a length-2 dragon cannot pay for an additional step.
- Collision checks occur before each movement step, including the own tail.
- Death causes are `Hit Wall`, `Hit Self`, `Hit Other Body`, `Head to Head`, and
	`No Valid Action`.
- Death drops pearls at every second body segment starting from the head.
- Sonar messages are unsigned 64-bit values. Up to four directional rays are
	cast after a surviving action, and messages arrive at the recipient's next
	turn. Rays stop at kelp or the first dragon and pass through portals.
- The judge budget is 100M CPU points and 48 MB per dragon. The sandbox is
	required for CPU accounting; unsandboxed runs only provide wall-clock checks.

## Fresh probe fixtures

The following fixtures were captured with fixed seeds and retain raw runner
stdout, stderr, and binary replay files:

| Fixture | Map | Seed | Sandbox | Candidate side | Rounds | Replay |
| --- | --- | ---: | --- | --- | ---: | --- |
| `phase0-arena-a` | arena, 11x11 | 0 | yes | A | 500 | format 2 |
| `phase0-arena-b` | arena, 11x11 | 1 | yes | B | 325 | format 2 |
| `phase0-portals-a` | portals, 32x16 | 2 | no | A | 500 | format 2 |
| `phase0-portals-b` | portals, 32x16 | 3 | no | B | 81 | format 2 |

The sandbox arena traces contain p50, p99, mean, maximum, and turn-count CPU
lines. The unsandboxed portal traces contain no CPU lines, as expected. The
fresh runs completed without runner errors.

## Replay format

The replay files are binary packed Cap'n Proto messages, not JSON. The current
parser decodes format 2 and exposes:

- map text, bot names, seed, winner, termination reason, and end-of-game team
	counts/lengths;
- round starts and turn starts;
- pearl countdown events and tile pearl changes;
- dragon actions, including multi-step moves, splits, and suicide;
- dragon head/tail updates and facing;
- split parent/child IDs, teams, and body coordinates;
- dragon deaths with the four observed collision causes.

The four fresh replay inventories contained 3,390 to 28,973 events each. The
observed death strings were `hit a wall`, `hit itself`, `hit another dragon`,
and `lost a head-to-head`. No `no valid action` death occurred in these runs.
Split events confirmed that child IDs are assigned separately and that parent
and child bodies are present in the replay event.

## Metric availability

| Metric | Source | Status | Notes |
| --- | --- | --- | --- |
| Winner, rounds, end reason | Runner/replay | Available | Parsed from documented result lines and replay result. |
| CPU p50/p99/mean/max/turns | Runner output | Available in sandbox | Null for unsandboxed games. |
| Raw death events | Runner/replay | Available | Runner lines and replay death events agree on observed causes. |
| Longest dragon at end | Replay result | Available | Stored per team. |
| Dragons at end | Replay result | Available | Stored per team. |
| Pearls collected | Replay events | Available/derived | Derived from pearl/tile and length events; validate against more fixtures. |
| Pearls total | Replay events | Approximate | Counts observed pearl appearances; drops and hidden initial state require explicit labeling. |
| Death categories | Death events + rules | Available | Unmatched causes remain unclassified. Attack attribution needs replay heuristics. |
| No-legal-move turns | Replay actions + map/body state | Not yet validated | Requires complete action legality reconstruction, including portals and wrapping. |
| Stuck loops | Dragon head updates | Available in principle | Requires loop detector validation on synthetic and real traces. |
| Length and unit curves | Replay updates/results | Available | Format 2 provides enough events for per-round reconstruction. |
| Cycle sealing | Map + simulator | Not available yet | Will be implemented and tested in the bodyguard validator. |
| Queen dash safety | Replay/simulator | Not available yet | Requires the Part 8 dash model and verified split/movement timing. |
| Per-turn governor budget | Sandbox runner output | Available for observed games | Strategy-side budget enforcement is not implemented yet. |

## Deferred probes and limitations

- The official documentation establishes the helper API, map-size range,
	coordinate wrapping, portal orientation, cooldown semantics, child
	orientation, and sonar timing. Dedicated adversarial probes for each edge
	case remain useful before relying on them in the simulator.
- The replay parser currently handles format 2 only. Unsupported future formats
	must remain unavailable rather than being guessed.
- The PATH `unswbc 1.2.2` versus project `unswbc 1.2.9` discrepancy should be
	normalized before contest submission or recorded in the run stamp.
- The current active bot is not yet the Part 0 recon bot described by
	`PROMPT.MD`; these fixtures characterize the engine and parser only.
# Discovery

Date: 2026-10-01

## Environment

- The active branch is `adarsh`, based on the existing initial commit.
- `unswbc` is not installed or available on `PATH`; fresh game fixtures and help output therefore could not be collected.
- Official CLI documentation says `unswbc run` accepts bot project directories and rebuilds each directory before a match. No staging workaround is required by the documented interface.
- `unswbc 1.2.2` emits packed Cap'n Proto replay messages, not JSON. The official viewer schema was verified against a 38,015-event replay fixture.

## Verified facts

- `--sandbox` is required for judge-style CPU accounting; local runs only have wall-clock limits.
- Documented death causes include `Hit Wall`, `Hit Self`, `Hit Other Body`, `Head to Head`, and `No Valid Action`.
- Bot directories contain `bot.toml` and source files.

## Deferred runtime checks

After installing `unswbc`, repeat the help commands, 3-4 real games with saved output/replays, replay inspection, a larger death-cause sample, and the active-bot deterministic regression check. The CLI reports replay-derived fields as `null` with a reason until that parser is verified.

## Metric availability

| Metric | Source | Status |
| --- | --- | --- |
| winner, rounds, end reason | runner output | available when documented lines match |
| CPU p50/p99/mean/max/turns | runner output | available with sandbox points lines |
| raw death events | runner output | available with documented death lines |
| longest dragon, dragons at end | replay GameResult | available |
| pearls collected/total | replay TileChange events | available; total is observed appearances including drops |
| death categories | replay death events + configured regex | available; unmatched causes remain unclassified |
| no-legal-move turns | replay actions + map edges/body state | available for replay format 2 |
| stuck loops | replay DragonUpdate head positions | available when qualifying repeats exist |
| length/unit curves | replay round and dragon events | available |
