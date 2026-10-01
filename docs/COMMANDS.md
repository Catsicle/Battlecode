# Battlecode Command Guide

This is the practical command reference for the current repository. All Python
commands use `uv`; run them from the repository root or from a subfolder.

## One-Time Setup

Run these once on a new machine or after cloning:

```powershell
uv sync
uv tool install unswbc
uv run bctool --help
uv run unswbc --version
uv run pytest -q
```

If `unswbc` is already installed as a uv tool, use:

```powershell
uv tool upgrade unswbc
```

The project invokes `unswbc` directly from `PATH`. It is not installed as a
project dependency because it is the game toolkit, not an analytics library.

## Logic-Only Workflow

Use this when you are changing bot behavior in `src/` and want the shortest
useful feedback loop.

### 1. Edit the bot

Only change the active bot files:

```text
src/main.cpp
src/helper.hpp       # organiser-provided; normally do not edit
src/bot.toml
```

The bot registry calls this bot `active`.

### 2. Run a smoke test

```powershell
uv run bctool smoke
```

Smoke runs two sandboxed games against `Random` on
`maps/train/arena.map`, one with the candidate as team A and one as team B.
It is the fastest check for compilation, crashes, timeouts, and obvious behavior
regressions.

### 3. Run a broader check when smoke passes

```powershell
uv run bctool quick
```

Quick uses the baseline and opponent groups, all train maps, seeds `0` through
`4`, both candidate sides, and a target budget of about 40 games. Because the
schedule preserves coverage and side balance, the actual number can be greater
than 40.

### 4. Read the generated report

The command prints the run directory. Open:

```text
results/runs/<run-id>/report.md
results/runs/<run-id>/summary.json
results/runs/<run-id>/games.jsonl
results/runs/<run-id>/replays/       # only when --keep-replays was used
```

Do not edit generated results. Make another source change, run smoke again, and
compare the new run with the previous one.

## Testing Commands

### List available bots

```powershell
uv run bctool list
```

The active bot is `active`. Other selectors can be individual bot IDs such as
`Random` or `Pearl1`, group names such as `baselines` and `opponents`, or nested
IDs such as `UselessBots/cpp`.

### Run a custom matchup

```powershell
uv run bctool run `
  --candidate active `
  --opponents Random,Pearl1 `
  --maps maps/train/arena.map `
  --seeds 0 1 2 `
  --jobs 4 `
  --keep-replays
```

This covers every selected opponent, map, and seed twice: candidate team A and
candidate team B.

### Select maps

All train maps:

```powershell
uv run bctool run --maps train
```

Holdout maps:

```powershell
uv run bctool run --maps holdout
```

Every train map plus holdout maps is not currently represented by a single
special selector; pass a glob or explicit comma-separated paths instead:

```powershell
uv run bctool run --maps "maps/train/*.map,maps/holdout/*.map"
```

A specific map:

```powershell
uv run bctool run --maps maps/train/portals.map
```

### Select opponents

```powershell
uv run bctool run --opponents Random
uv run bctool run --opponents baselines
uv run bctool run --opponents baselines,opponents
```

The candidate is automatically removed if it also appears in the opponent
selection.

### Control game count and repetition

```powershell
uv run bctool run --games-per-matchup 3
uv run bctool run --games 40
```

`--games-per-matchup` repeats each opponent/map/seed/side combination exactly
the requested number of times. `--games N` chooses the smallest repeated,
balanced schedule that covers all selected dimensions; it can exceed `N` when
coverage requires it.

### Parallelism and timeout

```powershell
uv run bctool run --jobs 1
uv run bctool run --jobs 8
uv run bctool run --timeout 180
```

Games use a thread pool because the work is subprocess-bound. `--jobs 1` is
useful when debugging a specific failure. The timeout is per game, in seconds.
Timeouts are recorded as `error`, not as bot losses.

### Sandbox mode

Sandbox is enabled by default for `run`, `smoke`, `quick`, and `full`:

```powershell
uv run bctool run
uv run bctool run --sandbox
```

To run without CPU metering:

```powershell
uv run bctool run --no-sandbox
```

Use unsandboxed mode only for fast local behavior checks. CPU gates fail when
CPU was not measured.

### Preview commands without playing

```powershell
uv run bctool run `
  --candidate active `
  --opponents Random `
  --maps maps/train/arena.map `
  --seeds 0 `
  --dry-run
```

This writes dry-run rows and reports, but does not invoke the game engine.

### Preserve replay and raw output

Keep replay files for visual inspection and replay-derived metrics:

```powershell
uv run bctool run --keep-replays
```

Keep runner stdout and stderr in each JSONL row:

```powershell
uv run bctool run --keep-output
```

These options can be combined:

```powershell
uv run bctool run --keep-replays --keep-output
```

Without `--keep-replays`, replay files are decoded and then deleted. The game
metadata remains in `games.jsonl`.

### Resume an interrupted run

```powershell
uv run bctool run --resume results/runs/<run-id>
```

Existing game indexes in `games.jsonl` are skipped. The remaining games are
appended as they finish.

## Analysis Commands

### Rebuild a run report

```powershell
uv run bctool report results/runs/<run-id>
```

This regenerates `report.md` and `summary.json` from `games.jsonl`.

### Evaluate gates

```powershell
uv run bctool gates results/runs/<run-id>
```

Current gates include runner errors and CPU measurement/limits. A missing or
unsandboxed CPU measurement is intentionally a failure.

### Rebuild the ELO leaderboard

```powershell
uv run bctool elo
```

Outputs are written under `results/`:

```text
results/elo_leaderboard.json
results/elo_leaderboard.md
```

### Find the exact command for a game

```powershell
uv run bctool replay-cmd results/runs/<run-id> 17
```

The number is the `index` field in `games.jsonl`. This prints the original
`unswbc run ...` command for that game.

### Inspect bot/replay identity

```powershell
Get-Content results/runs/<run-id>/games.jsonl |
  ConvertFrom-Json |
  Select-Object index,candidate,opponent,candidate_team,map,seed,replay
```

Replay filenames also contain the candidate, opponent, side, and seed, for
example:

```text
00001-active-vs-Random-A-0.replay
```

## Tier Summary

| Tier | Command | Use it for |
| --- | --- | --- |
| Smoke | `uv run bctool smoke` | Fast two-side sanity check |
| Quick | `uv run bctool quick` | Regular development validation |
| Full | `uv run bctool full --games 400` | Broad pre-submission evaluation |
| Custom | `uv run bctool run ...` | Exact opponent/map/seed control |

Equivalent Make targets:

```powershell
make setup
make test
make smoke
make quick
make full GAMES=400
make leaderboard
```

On Windows, use the `uv run bctool ...` commands directly if `make` is not
installed.

## Logic-Only Cheat Sheet

If you only want to work on bot logic, the commands you need are:

```powershell
uv sync
uv run bctool list
uv run bctool smoke
uv run bctool quick
uv run bctool report results/runs/<run-id>
```

You do not need to manually inspect Cap’n Proto files, calculate Wilson
intervals, classify deaths, or manage replay parsing. The runner and report
commands do that offline.

## Troubleshooting

- **`uv` not found:** install uv from [docs.astral.sh/uv](https://docs.astral.sh/uv/), then reopen the terminal.
- **`unswbc` not found:** run `uv tool install unswbc` and verify with `uv run unswbc --version`.
- **Stale environment:** run `uv sync`.
- **Compilation failure:** run `uv run bctool smoke --jobs 1 --keep-output` and inspect the stored stderr.
- **CPU not measured:** remove `--no-sandbox`; CPU gates require sandbox mode.
- **Replay metrics are null:** check the replay parser reason in `games.jsonl`; unsupported future replay formats remain unavailable rather than being guessed.
- **A run was interrupted:** rerun with `uv run bctool run --resume results/runs/<run-id>`.
