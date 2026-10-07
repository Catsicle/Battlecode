# Debugging the Battlecode bot

This guide explains how to inspect bot behavior without confusing a runner
failure with a game failure or corrupting the bot protocol.

The bot is compiled by `unswbc` to WebAssembly and then executed in a sandbox.
There is no normal interactive debugger attached to the C++ process during a
`bctool` match. Debugging is therefore based on:

1. deterministic reproduction;
2. compile-time debug instrumentation;
3. captured runner output;
4. replay files and replay-derived metrics;
5. small native test programs for internal values.

Run every command below from the repository root:

```powershell
Set-Location C:\Users\adars\yellow\battlecode\Battlecode
```

## 1. Understand the two layers

There are two separate things to debug.

### Native C++ code

This is the code compiled directly with `g++` or `clang++`. Use it for:

- `Memory`;
- `BodyTracker`;
- `resolve_step`;
- `Sim`;
- L1/L3/L4 validation;
- planner scoring;
- retry ordering.

Native tests can print values freely because they are not running inside the
judge protocol.

### The actual game bot

This is compiled by `unswbc` and runs one process per dragon. The bot must
produce valid protocol output. A normal turn must emit exactly one legal action
such as:

```text
MOVE N
PROTOCOL 3
```

Do not add ordinary `std::cout`, `printf`, or `std::cerr` calls to a per-turn
path in release code. The official performance guidance treats each output
write as expensive, and arbitrary stdout can interfere with protocol parsing.

## 2. First classify the failure

Always save both runner output and replays:

```powershell
uv run bctool run `
  --candidate active `
  --opponents Random `
  --maps maps/train/arena.map `
  --seeds 0 `
  --jobs 1 `
  --games-per-matchup 1 `
  --keep-replays `
  --keep-output
```

The command prints a run directory such as:

```text
results/runs/20261007T000000Z-<git-sha>
```

Inspect the report first:

```powershell
Get-Content .\results\runs\<run-id>\report.md
```

Then inspect the raw per-game records:

```powershell
Get-Content .\results\runs\<run-id>\games.jsonl |
  ConvertFrom-Json |
  Select-Object index,candidate,opponent,candidate_team,map,seed,status,rounds,end_reason,returncode
```

Interpret the result:

| Result | Meaning | Next action |
|---|---|---|
| `ERROR`, round `-` | Compile failure, runner failure, or timeout | Inspect `stderr`, `stdout`, and `returncode` in `games.jsonl` |
| `LOSS`, round `1` or `2` | Usually invalid protocol/action or immediate death | Run the exact command with `-v` |
| `LOSS`, later round | Valid game, but navigation or safety behavior failed | Inspect replay and add a focused trace |
| `WIN` or `LOSS`, round `500` | Normal completed game | Compare replay metrics and decisions |
| Timeout | Process exceeded the bctool timeout | Reproduce with `--jobs 1 --timeout 300 --verbose` |

To display the stored error fields for one game:

```powershell
$row = (Get-Content .\results\runs\<run-id>\games.jsonl |
  ConvertFrom-Json |
  Where-Object index -eq 1)
$row.stdout
$row.stderr
$row.returncode
$row.end_reason
```

## 3. Reproduce one exact game

Do not use a parallel run while debugging. Use:

```powershell
--jobs 1
```

`bctool` records the exact underlying command for every game. Retrieve it
with:

```powershell
uv run bctool replay-cmd .\results\runs\<run-id> 1
```

Copy the printed command and append `-v` before executing it directly. For
example:

```powershell
uv run unswbc run `
  maps/train/arena.map `
  src `
  bots/baselines/Random `
  --seed 0 `
  --no-logs `
  --no-indicator `
  --no-draw `
  --sandbox `
  --output .\results\debug-devil.replay `
  -v
```

The direct command is useful because it prints per-round bot protocol output.
For an immediate-action failure, look for:

```text
MOVE N
MOVE E
MOVE S
MOVE W
SPLIT 2
```

An invalid direction byte, missing action, malformed protocol line, or compile
diagnostic identifies a protocol/runner problem rather than a path-scoring
problem.

Use `--sandbox` when reproducing judge behavior. Use `--no-sandbox` only for a
fast local check; it does not measure CPU and can behave differently.

## 4. What each runner option does

### `--verbose` / `-v`

Streams runner output while the game is running:

```powershell
uv run bctool run `
  --candidate active `
  --opponents Random `
  --maps maps/train/arena.map `
  --seeds 0 `
  --jobs 1 `
  --verbose
```

Use this for:

- compile errors;
- invalid actions;
- the exact round where behavior changes;
- timeouts;
- checking whether `MOVE` or `SPLIT` was emitted.

### `--keep-output`

Stores captured stdout and stderr in `games.jsonl`. Without this option,
`bctool` still uses the output to classify the result but does not retain the
full text in the row.

### `--keep-replays`

Keeps replay files under:

```text
results/runs/<run-id>/replays/
```

Without this option, bctool may decode the replay and then delete it.

### `--jobs 1`

Runs games serially. Always use it while debugging so the output and result
index are unambiguous.

### `--timeout`

The timeout is per game:

```powershell
uv run bctool run ... --jobs 1 --timeout 300 --verbose
```

Do not increase the timeout to hide a CPU regression. First determine whether
the bot is looping or simply producing slow search work.

## 5. Add safe bot instrumentation

The preferred debugging design is a compile-time gate. Add the gate to
[`config.hpp`](../src/config.hpp), not as a runtime command-line option:

```cpp
#ifdef NAV_DEBUG
inline constexpr bool NAV_DEBUG_ENABLED = true;
#else
inline constexpr bool NAV_DEBUG_ENABLED = false;
#endif
```

Use a tiny helper in [`log.hpp`](../src/core/log.hpp). Debugging output must
not be enabled in normal submissions:

```cpp
#ifdef NAV_DEBUG
#include <cstdio>
#define NAV_TRACE(...) std::fprintf(stderr, __VA_ARGS__)
#else
#define NAV_TRACE(...) do { } while (false)
#endif
```

Keep traces:

- behind `#ifdef NAV_DEBUG`;
- outside the normal build;
- limited to a small number of turns;
- written to stderr rather than protocol stdout;
- free of heap-heavy formatting in hot loops;
- disabled before normal smoke or tournament runs.

Do not print an entire 64x64 map or every recursive node. That can exceed the
CPU budget and make the output unusable.

### Limit tracing to selected turns

Use a turn filter so a trace does not run for the entire game:

```cpp
#ifdef NAV_DEBUG
if (round >= 20 && round <= 25) {
    NAV_TRACE("round=%d head=(%d,%d) length=%d\n",
              round, controller.get_position().x,
              controller.get_position().y, controller.get_length());
}
#endif
```

For a dragon-specific trace, also filter by controller id:

```cpp
#ifdef NAV_DEBUG
if (controller.get_id() == 0 && round >= 20 && round <= 25) {
    NAV_TRACE("id=%d round=%d\n", controller.get_id(), round);
}
#endif
```

Avoid using global debug state to identify a dragon. Each dragon is a separate
process, and state is not shared between dragons.

## 6. What values to print

When a move is surprising, inspect values in this order.

### Turn and controller state

Print:

```text
round
controller id
position
length
current direction
map width and height
```

This confirms that the observation and the assumed coordinate system match.

### Memory state

For the current head and each candidate destination, print:

```text
cell index
known
edge known
kelp flag
portal id
pearl flag
eaten_here
visits_nopearl
last_visit_round
occupancy.present
occupancy.head
occupancy.round
occupancy.segment_index
occupancy.owner_length
```

The most common diagnostic mistake is reading `occupancy.present` without
checking freshness:

```cpp
bool fresh = memory.occ_fresh(cell, memory.current_round);
```

### Body tracker

Print the predicted body before and after reconciliation:

```text
predicted body: head -> segment 1 -> segment 2 ...
observed visible own cells
controller length
rebuild count
mismatch count
```

If `BodyTracker` is wrong, `Sim::seg_index()` can reject a safe move or permit
a self-collision.

### Transition result

For each candidate direction, print:

```text
direction
source cell
destination cell
new direction
StepKind
```

The only valid transition kinds are:

```text
Normal
PortalOK
PortalUnresolved
Kelp
```

Never duplicate portal, wrapping, or kelp logic in a debug-only path. Call
`resolve_step()` itself and print its result.

### L1 result

Print the exact failure returned by `Sim::try_step()`:

```text
OK
KELP
SELF
OTHER_BODY
HEAD_ON
PORTAL_UNRESOLVED
DEAD_END
```

For enemy-head issues, also print:

```text
enemy cell
enemy head flag
enemy observation round
current round
freshness result
toroidal distance to candidate destination
DEFAULT_ENEMY_REACH
```

### Planner result

For the selected route, print only:

```text
mode
target, if any
path found
path length
path cost
first direction
destination of first step
forbidden-first-direction mask
```

For Explore mode, print the selected cell and its score components:

```text
distance cost D(v)
pearl reward
frontier reward
stale reward
no-pearl visit penalty
final score S(v)
```

### Retry result

When a route is rejected, print:

```text
attempt number
prefix length
Fail value
first direction
forbid mask after retry
```

This distinguishes:

- a planner that selected a bad route;
- a validator that rejected a correct route;
- a fallback that emitted an unexpected direction.

## 7. Use a debug build without changing the judge command

The simplest approach is to temporarily add a debug macro to the bot build
only when compiling locally. For a native diagnostic binary:

```powershell
g++ -std=c++20 -O0 -g -DNAV_DEBUG `
  .\src\main.cpp `
  .\src\core\memory.cpp `
  .\src\core\body_tracker.cpp `
  .\src\comms\comms.cpp `
  .\src\nav\grid.cpp `
  .\src\nav\bfs.cpp `
  .\src\nav\astar.cpp `
  .\src\nav\flee.cpp `
  .\src\nav\wander.cpp `
  .\src\nav\transition.cpp `
  .\src\nav\sim.cpp `
  .\src\nav\safety.cpp `
  .\src\nav\planner.cpp `
  .\src\nav\executor.cpp `
  .\src\roles\explorer.cpp `
  .\src\roles\killer.cpp `
  .\src\roles\feeder.cpp `
  .\src\roles\bodyguard.cpp `
  .\src\roles\queen.cpp `
  .\src\roles\stub.cpp `
  .\src\roles\registry.cpp `
  -o .\battlecode-debug.exe
```

The native executable is useful for unit-style fixtures, but it is not a
substitute for a sandbox replay. The actual engine supplies controller state
and protocol timing, so always confirm the final behavior with `unswbc`.

For WebAssembly/judge compilation, use the normal runner after enabling the
debug gate, but keep the trace window very small:

```powershell
uv run bctool run `
  --candidate active `
  --opponents Random `
  --maps maps/train/arena.map `
  --seeds 0 `
  --jobs 1 `
  --keep-output `
  --keep-replays `
  --verbose
```

After debugging, disable `NAV_DEBUG` and remove or retain only the gated
diagnostic code. Never leave per-turn logging enabled in the release bot.

## 8. Native tests for internal values

Use a small plain C++ test when the value can be tested without the engine:

- construct a `Memory` object;
- mark known cells and edges;
- add pearls, portals, or occupancy;
- construct a `Sim`;
- call `resolve_step`, `try_step`, `undo`, `trap_ok`, or `survives`;
- use `assert` or a tiny `CHECK` macro.

Run the existing focused test:

```powershell
.\tests\run_tests.ps1
```

Add a regression test whenever a bug is found. Useful test cases include:

```text
enemy head within DEFAULT_ENEMY_REACH -> HEAD_ON
stale enemy occupancy -> not treated as a blocker
unknown portal -> PORTAL_UNRESOLVED
kelp edge -> KELP
tail/hairpin collision -> SELF
pearl growth -> body remains occupied for the correct extra turn
failed long prefix, safe short prefix -> retry shortens first
```

Native tests should not depend on `bctool`, `unswbc`, a replay file, or a
network connection.

## 9. Replay inspection

Use the replay retained by bctool:

```powershell
Get-ChildItem .\results\runs\<run-id>\replays
```

The replay is the source of truth for what the engine actually did. Compare
it with the bot's intended decision trace:

| Trace value | Replay question |
|---|---|
| selected direction | Did the dragon actually move that direction? |
| predicted head | Does the next replay frame show that head position? |
| predicted length | Did a pearl cause growth? |
| selected split | Did the engine create the expected child? |
| enemy occupancy | Was the enemy visible in the relevant frame? |
| round number | Did the failure occur on the same turn as the trace? |

Use the stored command to reproduce a replay exactly:

```powershell
uv run bctool replay-cmd .\results\runs\<run-id> 1
```

Do not infer a current source bug from an old replay. Check the `bot_hash`,
`git_sha`, `dirty`, map, seed, side, and command in `games.jsonl`.

## 10. A reliable debugging loop

Use this loop for every behavior bug:

1. Make the smallest source change that adds a test or gated trace.
2. Compile the complete source list with warnings enabled.
3. Run `.\tests\run_tests.ps1`.
4. Run one exact matchup with `--jobs 1 --seeds 0`.
5. Add `--keep-output --keep-replays`.
6. If the failure is early, reproduce with `unswbc ... -v`.
7. Compare the trace round with the replay round.
8. Identify the first incorrect value, not the final bad move.
9. Add a regression test for that value.
10. Remove or disable debug output.
11. Run smoke again with normal release settings.
12. Run the broader matchup only after the focused case is fixed.

Recommended focused command:

```powershell
uv run bctool run `
  --candidate active `
  --opponents PortalNoLimit `
  --maps maps/train/devil.map `
  --seeds 0 `
  --jobs 1 `
  --games-per-matchup 1 `
  --timeout 300 `
  --keep-output `
  --keep-replays `
  --verbose
```

## 11. Common mistakes

### Debugging with `bctool smoke` only

Smoke runs two sides and can hide which side failed. Use `run --jobs 1` for
the exact map, opponent, seed, and side.

### Forgetting `--keep-output`

Then the important compiler or runner text is not retained in the JSONL row.

### Forgetting `--keep-replays`

Then the replay may be deleted after metrics are collected.

### Running with multiple jobs

Output and failures interleave. This makes a deterministic bug appear random.

### Printing values to stdout

Protocol output can become invalid. Use compile-time-gated stderr diagnostics
only for a short investigation, and keep release builds silent.

### Debugging an old replay

Verify `bot_hash`, `git_sha`, `dirty`, source path, seed, and command before
trusting the replay.

### Treating a stale enemy observation as current

Always inspect both:

```cpp
memory.occupancy[cell].round
memory.current_round
memory.occ_fresh(cell, memory.current_round)
```

### Changing several safety layers at once

Change one layer, add one focused test, and rerun the same replay. Otherwise
the first incorrect value is difficult to locate.

## 12. Before committing a debugging change

Run:

```powershell
g++ -std=c++20 -O2 -Wall -Wextra -pedantic ...
.\tests\run_tests.ps1
git --no-pager diff --check
uv run bctool smoke --jobs 1 --keep-replays --keep-output
```

Confirm that:

- no unguarded debug output remains;
- no protocol output is changed;
- all new source files are listed in `src/bot.toml`;
- the replay and report correspond to the final source;
- debug macros are disabled for normal runs;
- performance remains within the expected CPU budget.

