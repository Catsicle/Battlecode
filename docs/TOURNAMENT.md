# Bot Tournament Framework

`tournament.py` runs the selected bot against every other discovered bot directory and produces both a readable summary and structured result files.

## Bot discovery

The framework scans the workspace root for directories containing:

- `bot.toml`
- one of `main.cpp`, `main.cc`, `main.c`, or `main.py`

The current workspace therefore includes bots such as `Pearl1`, `Pearl2`, `Utility`, `cpp`, `killmode`, and `mybot` when they are present. It does not require a manually maintained bot list.

## Basic commands

From `/home/catsicle/Battlecode`:

```sh
python3 tournament.py --list-bots
```

Run `Pearl2` against every other discovered bot on `arena.map`, using seed `0`:

```sh
python3 tournament.py --candidate Pearl2
```

Use multiple maps and seeds:

```sh
python3 tournament.py \
  --candidate Pearl2 \
  --maps maps/arena.map maps/portals.map maps/default_small.map \
  --seeds 0 1 2 3 4
```

Run only selected opponents:

```sh
python3 tournament.py \
  --candidate Pearl2 \
  --opponents Pearl1 mybot cpp
```

Repeat every matchup several times with the same map/seed schedule:

```sh
python3 tournament.py --candidate Pearl2 --games-per-matchup 3
```

Use the judge-style WebAssembly CPU and memory accounting:

```sh
python3 tournament.py --candidate Pearl2 --sandbox
```

Preview every command without running matches:

```sh
python3 tournament.py --candidate Pearl2 --dry-run
```

Save one replay for every game:

```sh
python3 tournament.py --candidate Pearl2 --keep-replays
```

The default per-game timeout is 120 seconds. Change it when testing a slow local build:

```sh
python3 tournament.py --candidate Pearl2 --timeout 300
```

## Scheduling

Each candidate/opponent/map/seed combination is played twice:

1. Candidate as team A.
2. Candidate as team B.

This reduces team-side bias and makes the report distinguish performance from ordering effects.

For example, two opponents, two maps, and three seeds produce:

`2 opponents × 2 maps × 3 seeds × 2 sides = 24 games`

## Reported statistics

The terminal report includes:

- total games,
- wins, losses, draws, and runner errors,
- overall win rate,
- decisive-game win rate,
- average, minimum, and maximum rounds,
- wins and losses grouped by opponent,
- team-A versus team-B performance,
- observed death causes,
- per-game status and round count.

The runner's CPU statistics are also parsed when `--sandbox` is used, including p50, p99, mean, maximum points, and turn counts for each team. These are retained in the JSON game records.

## Output files

Results are written to `tournament_results/` by default:

- `*.json`: complete summary plus every game record, command, status, deaths, CPU data, and optional raw output.
- `*.csv`: spreadsheet-friendly one-row-per-game data.
- `replays/`: optional replay files when `--keep-replays` is enabled.

Use `--output-dir results/my-experiment` to keep separate experiments:

```sh
python3 tournament.py \
  --candidate Pearl2 \
  --maps maps/arena.map maps/portals.map \
  --seeds 10 11 12 \
  --sandbox \
  --keep-replays \
  --output-dir results/pearl2-baseline
```

## Interpreting failures

An `error` means the runner did not produce a recognizable win/draw result, timed out, or exited unsuccessfully. This is kept separate from a game loss so build failures and bot crashes do not silently reduce the win rate.

For CPU investigations, compare the sandbox p99 and max points against the documented per-dragon budget. For behavior investigations, use the saved replay path from the JSON record and inspect the `deaths` list for wall, self, head-to-head, and other collisions.

## Adding another historical bot

Place the bot in its own directory under the workspace root with its normal `bot.toml` and `main.*`. The next `--list-bots` or tournament run discovers it automatically.