# MASTER PROMPT — Battlecode (UNSW `unswbc`) workspace overhaul

You are a senior engineer working inside my VS Code workspace `Battlecode/`. Your job is to reorganise the repository, add git-based reproducibility, and build a professional testing/analytics toolchain for my bot. **Work in phases, commit after each phase, and keep every existing capability working.** Read this whole document before touching anything.
---

## 0. Ground rules

1. **First read** `Ally/agents.md`, `Ally/IMPLEMENTATION.md`, `TOURNAMENT.md` and `tournament.py`. If `agents.md` contains rules, obey them and keep that file (move it to the repo root as `AGENTS.md` if it is an agent-instruction file).
2. **Never modify `helper.hpp`** (organiser-provided) and **never change bot behaviour** (no edits to the logic in any bot's `main.cpp`) except the mechanical split described in Phase 1.
3. **Never delete data.** Use `git mv` / plain moves. Old replays and results go to `results/legacy/`.
4. **Do not add per-turn logging or telemetry to the bot code.** In the judge, each stdout/stderr write costs 2,500,000 CPU points (+4,000/byte) of a 100,000,000 per-dragon-per-turn budget. All analytics must be derived **offline** from runner output and replay files.
5. **Do not guess formats.** The engine output and replay format are not documented in this prompt. Phase 0 (Discovery) must establish them from real runs. Any metric that cannot be derived must be reported as `null` with a clear reason in the report (never invent numbers, never silently print 0).
6. Prefer Python 3.10+ standard library. Allowed extra deps: `matplotlib`, `rich` (optional, fall back to plain text), `pytest`, `tomli` (only if Python < 3.11). List them in `pyproject.toml`.
7. **Cross-platform.** I am likely on Windows (my source files use CRLF). The primary interface must be the Python CLI (`uv run bctool ...`), which works everywhere. The `Makefile` is a thin convenience wrapper; also add `.vscode/tasks.json` entries so I can run everything from VS Code without `make`. Use `pathlib`, no shell-only features, no symlinks (Windows) — use a `latest.txt` pointer file instead.
8. At each phase end: run the tests you wrote, make a git commit with a clear message, and print a 3–5 line status. If something is ambiguous, choose the most reasonable option and record it in `docs/DECISIONS.md` (decision, alternatives, reason). Do not stop to ask me unless you are blocked.

---

## 1. Context you must know

- Game: UNSW Battlecode, runner CLI `unswbc`. Teams of "dragons" (snake-like) collect pearls to grow. **The winner is determined by the longest dragon at the end** (docs: https://game.battlecode.au/docs/overview). Up to 64 dragons per team, max 500 rounds, toroidal map, vision 7×7, bots can split, sonar is the only dragon-to-dragon communication.
- Useful docs to read in Phase 0: `/docs/cli`, `/docs/death`, `/docs/pearls`, `/docs/splitting`, `/docs/movement`, `/docs/timeouts`, `/docs/map-files`, `/docs/protocol`, `/docs/execution-order`, `/docs/game-format` (all under `https://game.battlecode.au`).
- Local run: `unswbc run <map> <botA> <botB> --seed N [--sandbox] [--no-logs] [--no-indicator] [--no-draw] [--output replay | --no-replay]`.
  - Without `--sandbox` there is **no CPU metering** (only a 10 s wall clock). `--sandbox` prices CPU exactly like the judge (limit **100M points per dragon per turn**, 48 MB memory). Games are deterministic for a given seed + sandbox.
- `tournament.py` already parses these runner lines (keep these regexes as the starting point, verify them in Phase 0):
  ```python
  WIN_RE    = r"team ([AB]) wins after (\d+) rounds(?: \(([^)]+)\))?"
  DRAW_RE   = r"draw after (\d+) rounds(?: \(([^)]+)\))?"
  POINTS_RE = r"team ([AB]) points per turn:\s+p50\s+([\d.]+M)\s+p99\s+([\d.]+M)\s+mean\s+([\d.]+M)\s+max\s+([\d.]+M)\s+\((\d+) turns\)"
  DEATH_RE  = r"round (\d+): bot (\d+) \(team ([AB])\) died: (.+?)\s*$"
  ```
- Known bugs in the current `tournament.py` that must be fixed in the new runner:
  1. On `subprocess.TimeoutExpired`, `exc.stdout/stderr` may be `bytes`; storing them in the result and calling `json.dumps` crashes the whole run. Decode safely and respect `keep_output`.
  2. Results are written only at the very end, so one crash loses everything. Write **one JSONL line per game as it finishes**.
  3. Games run sequentially. Run them in parallel.
  4. `--sandbox` is off by default, so CPU stats are usually empty. Tiers `smoke/quick/full` and gates must run with sandbox **on**.
  5. No per-map breakdown, no confidence intervals.

---

## 2. Phase 0 — Discovery (do this first, produce `docs/DISCOVERY.md`)

1. Run `unswbc --help`, `unswbc run --help`, and read the docs pages listed above.
2. Check whether `unswbc run` accepts bot **directory paths** like `bots/opponents/Pearl1` (not just names relative to cwd). If it only accepts names relative to cwd, implement a **staging step**: copy/junction the chosen bot dirs into a temp dir (e.g. `.stage/<id>/`) and run from there. Record the finding.
3. Check whether the judge compiles **multiple `.cpp` files** or only `main.cpp`. If unsure, the safe rule is: **single translation unit** — split bot logic into **headers** (`pathfinding.hpp`, `strategy.hpp`) included from `main.cpp`. Record the finding in `docs/DECISIONS.md`.
4. Run 3–4 real games (different maps, both sides, with and without `--sandbox`, with `--output` to save a replay) and **save the raw stdout/stderr and replay files** into `tests/fixtures/`. Inspect the replay file format (text/JSON/binary?). Write down exactly what per-round data it contains: positions/ids of every dragon part, lengths, pearls, deaths, death causes, unit counts, etc.
5. Collect all distinct **death cause strings** from runner output across many games (play ~30 quick games vs different bots). Save them in `docs/DISCOVERY.md`; they drive the death classifier (Phase 4).
6. Produce a **Metric Availability Table** in `docs/DISCOVERY.md`: for every metric in Phase 4, state `from runner output` / `from replay` / `derived` / `NOT AVAILABLE` and how. Do not proceed to implement a metric until this table says how it will be computed.

---

## 3. Phase 1 — Git + repository restructure

### 3.1 Git setup
- `git init` if there is no repo. Create `.gitignore`:
  ```
  results/
  !results/.gitkeep
  .stage/
  .unswbc-build/
  **/.unswbc-build/
  __pycache__/
  *.pyc
  .pytest_cache/
  *.zip
  *.wasm
  build/
  .venv/
  ```
- Create `.gitattributes` with `* text=auto` and `*.cpp *.hpp *.py *.md *.toml text` to avoid CRLF/LF noise. **Do not rewrite line endings of existing files in bulk** except as git normalisation does on its own.
- Initial commit of the **current state before moving anything** (message: `baseline: before restructure`). Then tag `v0-baseline`.

### 3.2 Target layout
Use this tree (adapted from my sketch; deviations are intentional and must be justified in `docs/DECISIONS.md`):

```
Battlecode/
├── src/                      # ACTIVE bot — itself a valid unswbc bot directory
│   ├── main.cpp              # thin: init/update loop, calls into modules
│   ├── pathfinding.hpp       # (header-only modules unless Phase 0 proves multi-.cpp works)
│   ├── strategy.hpp
│   ├── helper.hpp            # organiser file, untouched
│   └── bot.toml              # needed so `src` is a runnable bot dir
├── bots/
│   ├── baselines/            # Random, UselessBots, ExploreUnseen, Length, mybot(?) ...
│   ├── opponents/            # Pearl1, Pearl2, Split, SplitHardLimit, SplitNoLimit, PortalHardLimit, PortalNoLimit
│   └── history/              # frozen versions of my active bot: v0-baseline, v1.0 ...
├── maps/
│   ├── train/                # official maps (move current maps/* here)
│   └── holdout/              # empty for now + README: "never tune on these"
├── scenarios/                # tiny hand-made maps + expected outcomes (Phase 9)
├── config/
│   ├── gauntlet.toml         # opponents, map sets, seeds, tiers, gates
│   └── death_rules.toml      # death-cause regex -> category
├── tools/bctool/             # THE python package (single source of truth)
│   ├── __main__.py  cli.py  registry.py  runner.py  parse.py  replay.py
│   ├── metrics.py  loops.py  deaths.py  stats.py  elo.py  gates.py
│   ├── report.py  plots.py  gitinfo.py  snapshot.py  scenarios.py
├── scripts/                  # thin wrappers kept for muscle memory
│   ├── tournament.py         # calls bctool.cli: `uv run bctool run`
│   ├── elo.py                # calls bctool.cli: `uv run bctool elo`
│   └── analyze.py            # calls bctool.cli: `uv run bctool report`
├── tests/                    # pytest + fixtures/
├── results/                  # generated, git-ignored
│   ├── runs/<YYYYmmddTHHMMSSZ>-<gitsha>/   # games.jsonl, summary.json, report.md, replays/, plots/
│   ├── bots/<bot-id>/        # per-bot cumulative report.md, stats.json, plots/
│   ├── latest.txt            # path of the most recent run dir (no symlinks)
│   ├── elo_leaderboard.json  # + elo_leaderboard.md
│   └── legacy/               # old replays/ and results/ contents moved here
├── docs/                     # my notes (from `Docs/`), IMPLEMENTATION.md, TOURNAMENT.md, USAGE.md, DECISIONS.md, DISCOVERY.md
├── CMakeLists.txt            # IDE/IntelliSense/clang-tidy ONLY (C++20). Not used by the judge.
├── Makefile                  # thin wrapper over `uv run bctool`
├── pyproject.toml
├── AGENTS.md                 # if applicable
└── README.md
```

Notes on deviations (document them): `bot.toml` lives in `src/` (not the repo root) because the runner needs a directory containing both `bot.toml` and `main.cpp`; `scripts/` is only thin wrappers because all logic lives in `tools/bctool/` (avoids duplicated code); `results/` appears once.

### 3.3 Folder mapping (verify each by reading it; adjust if I'm wrong)
- `Ally/` → `src/` (split `main.cpp` mechanically into `main.cpp` + `pathfinding.hpp` (board build, portal/move destination, simulate_move, search) + `strategy.hpp` (evaluate, choose_move, split rule). Keep the behaviour byte-for-byte equivalent: same constants, same order of operations. After splitting, run the same seed(s) before/after and confirm **identical game results** — this is your regression proof; record it in the commit message.)
- `Random`, `UselessBots`, `ExploreUnseen`, `Length`, `mybot` → `bots/baselines/` (inspect `mybot`; if it is an older version of my bot, put it in `bots/history/`).
- `Pearl1`, `Pearl2`, `Split`, `SplitHardLimit`, `SplitNoLimit`, `PortalHardLimit`, `PortalNoLimit` → `bots/opponents/`.
- `Docs/` → `docs/`; `Ally/IMPLEMENTATION.md`, `TOURNAMENT.md` → `docs/`.
- `maps/*` → `maps/train/`; top-level `replays/` and old `results/` → `results/legacy/`.
- `submission2pnl.zip` → `bots/history/submissions/` (git-ignored by `*.zip`; mention in README).
- Remove stale `.unswbc-build` dirs from tracking (they are build output).

### 3.4 CMakeLists.txt
Minimal, for editor tooling only: C++20 (`helper.hpp` uses defaulted `operator==`), include dir `src`, one executable target `bot` from `src/main.cpp`. Add a comment: "the judge compiles to wasm itself; this file is only for IntelliSense."

### 3.5 Git stamping (reproducibility)
Implement `tools/bctool/gitinfo.py`:
- `git_sha()` → short SHA of `HEAD` (or `"nogit"` if not a repo).
- `dirty()` → `True` if `git status --porcelain` is non-empty (ideally limited to the bot dir being tested).
- `bot_hash(bot_dir)` → first 10 hex chars of SHA-256 over all `*.cpp, *.hpp, *.h, *.c, *.py, *.toml` files in the bot dir, **sorted by relative path, with line endings normalised (`\r\n` → `\n`)** so Windows/Linux hashes match. Include relative paths in the hash input.
- Every run records, in `summary.json` and in each `games.jsonl` row: `git_sha`, `dirty`, `bot_hash` for the candidate **and each opponent**, `unswbc --version` (if available), config hash, python version, timestamp.
- Run directory name: `results/runs/<UTC timestamp>-<git_sha>[-dirty]/`.
- Print a loud warning if `dirty` is true for the candidate: "uncommitted changes — SHA does not describe the code; bot_hash does."

---

## 4. Phase 2 — Bot registry & config

`registry.py`: scan `src/` (id `active`) and `bots/*/*/` for dirs containing `bot.toml` + a main source file. Each bot has: `id` (folder name; for `src` use `active`), `group` (`active|baselines|opponents|history`), `path`, `bot_hash`. Support selectors on the CLI: a bot name, a group name (`baselines`, `opponents`, `history`), `all`, or comma lists; `--exclude`.

`config/gauntlet.toml` (create with sensible defaults; all values overridable on CLI):
```toml
[defaults]
jobs = 0                 # 0 = cpu_count - 1
sandbox = true
timeout_seconds = 120
seeds = [0, 1, 2, 3, 4]
games_per_matchup = 1    # each matchup is played on BOTH sides (A and B)

[maps]
train = "maps/train/*.map"
holdout = "maps/holdout/*.map"

[opponents]
quick = ["baselines", "opponents"]   # selectors
full  = ["baselines", "opponents", "history"]

[tiers.smoke]   # Tier 0: seconds
maps = ["first-train-map"]; opponents = ["Random"]; seeds = [0]; games_per_matchup = 1

[tiers.quick]   # Tier 1: ~40 games
games = 40      # total games target; runner picks maps/opponents/seeds to reach it, balanced over sides

[tiers.full]    # Tier 2: 400+ games; `--games N` overrides
games = 400

[gates]         # hard gates, evaluated after smoke/quick/full/presubmit
max_errors = 0                    # any crash/timeout/runner error fails the gate
max_cpu_points_M = 50             # FAIL if candidate max points-per-turn > 50M (judge limit is 100M)
warn_cpu_p99_M = 30               # WARN above this
max_unclassified_death_share = 0.2  # WARN if too many deaths can't be classified
must_beat_baselines = true        # Wilson 95% lower bound of decisive win rate vs each baseline > 0.5
vs_previous_snapshot_min_winrate = 0.5  # point estimate vs newest history bot must be >= this (no regression)
```
Define clearly (in code comments and `docs/USAGE.md`) how `games` is distributed over opponents × maps × seeds × sides so that **every opponent and every map is covered and sides are balanced**.

---

## 5. Phase 3 — Runner (`runner.py`, `cli.py`)

Implement `uv run bctool run` with: `--candidate` (default `active`), `--opponents`, `--maps` (`train|holdout|all|paths|globs`), `--seeds`, `--games-per-matchup`, `--games N` (cap/budget), `--jobs J`, `--sandbox/--no-sandbox`, `--keep-replays`, `--keep-output`, `--timeout`, `--dry-run`, `--tag NAME` (free-text label in summary), `--out DIR`. Keep **all old `tournament.py` flags working** via `scripts/tournament.py` (map old → new names).

Requirements:
- Parallel execution with `ThreadPoolExecutor` (subprocess-bound). Replays always requested internally (needed for telemetry) unless `--no-replay-analysis`; delete replays after analysis unless `--keep-replays`. Make replay file names unique per game.
- Each finished game appends one row to `games.jsonl` immediately (atomic append, flush). The run is **resumable**: `uv run bctool run --resume <run-dir>` skips games already present.
- Each row contains: ids, bot hashes, map, seed, side of candidate, status (`win|loss|draw|error`), winner, rounds, end reason, returncode, duration, CPU points per team (p50/p99/mean/max/turns as floats in millions), the full metrics block from Phase 4 **for both teams**, raw death events, replay path (if kept), and git stamp.
- **Live progress line after EVERY game** (this is where I get the Wilson interval):
  ```
  [17/120] active vs Pearl2 | arena.map seed 3 side B | WIN r=500 | longest 31 vs 18 | CPU max 42.1M
           vs Pearl2: 9W-6L-2D  decisive win 60.0%  Wilson95 [35.7%, 80.2%]  |  overall 61.2%  Wilson95 [52.0%, 69.6%]
  ```
  Wilson is computed on **decisive games (W / (W+L))**, draws reported separately (also show the score with draws = ½). Implement in `stats.py`:
  ```python
  def wilson(w, n, z=1.96):
      if n == 0: return (0.0, 1.0)
      p = w/n; d = 1 + z*z/n
      c = (p + z*z/(2*n))/d; h = z*((p*(1-p)/n + z*z/(4*n*n))**0.5)/d
      return (c-h, c+h)
  def elo_diff(p): ...  # 400*log10(p/(1-p)), guard p in (0,1)
  ```
- Fix the timeout/bytes bug. Mark games that exceed the wall timeout as `error`.
- **Do not implement SPRT.** (Leave a `# TODO(sprt)` comment in `stats.py` only.)

---

## 6. Phase 4 — Per-game telemetry (`metrics.py`, `deaths.py`, `loops.py`, `replay.py`)

Compute, **for both teams in every game**, from runner output + replay (see Metric Availability Table from Phase 0). If a value is unavailable, store `null` plus a `reason` string.

### 6.1 End-of-game metrics
| Key | Definition |
|---|---|
| `longest_dragon_end` | length of the team's longest living dragon at game end (**this decides the winner**; also store the opponent's and the margin) |
| `dragons_end` | number of living dragons at game end |
| `pearls_collected` | pearls eaten by the team (sum of growth events) |
| `pearls_total` | total pearls that existed in the game (initial + respawned) so I can show `collected / total` and `%`. If the replay can't give the total, report the best available definition (e.g. initial count) and **label it explicitly**. |
| `deaths_total` | dragons of the team that died |
| `deaths_suicide` / `deaths_attacked` / `deaths_other` / `deaths_unclassified` | classification below |
| `no_legal_move_turns` | number of dragon-turns in which the dragon had **zero** legal moves (all four neighbours blocked by kelp/body/other dragon, accounting for portals & wrap-around) |
| `stuck_loop_turns` | number of dragon-turns spent inside a detected loop (below) |
| `loops_count` | number of distinct loop episodes |
| `loops` | list of `{dragon_id, start_round, end_round, period, repeats, persisted_to_end_or_death}` |
| `loop_len_mean/max` | mean/max of `period` over episodes |
| `cpu_p50/p99/mean/max_M` | from the sandbox points line |

### 6.2 Death classification (`config/death_rules.toml`)
Build a rule table mapping **regexes over the death-cause strings seen in Phase 0** to categories:
- `suicide` = self-inflicted: hit own body, hit kelp/wall, no legal move, CPU/timeout/crash of own program, etc.
- `deliberate_attack` = caused by an enemy dragon acting on purpose, e.g. an enemy head moved into a position that killed the dragon (head-on loss), the enemy cut it off and it was forced into a collision. Use the replay to disambiguate when the cause string alone is not enough (e.g. enemy head adjacent in the previous round + enemy moved into the victim's path). Document the exact heuristic in code comments and `docs/USAGE.md`.
- `other` = anything explained but neither of the above (e.g. starvation/environment rules if any).
- `unclassified` = no rule matched. Always list unmatched cause strings in the report so I can add rules. **Never** silently bucket unknowns into `suicide`.

### 6.3 Loop detection (`loops.py`)
Per dragon, using the head-position sequence (include portal jumps; positions wrap):
- A **stuck loop** = for some period `p` in `[2, 64]`, the last `k·p` head positions repeat with `k ≥ 3` cycles **and no pearl was eaten during those cycles** (length unchanged).
- Detect episodes greedily (smallest period first), merge overlapping detections, record start/end round, period, repeats.
- An **"infinite loop"** = an episode that lasts ≥ 50 rounds or persists until the dragon dies/the game ends (`persisted_to_end_or_death = true`). Both numbers are reported (all loops vs infinite loops) with their lengths (period).
- Unit-test it on synthetic sequences (period 2, 4, 8; loop with a pearl eaten inside = not a loop; non-repeating path).

### 6.4 Time series (per team, per round)
Store compactly in the game row (or a sidecar `curves/<game>.json`): `longest_len[r]`, `total_len[r]`, `unit_count[r]` for r = 1..rounds. Used for the length curve and unit-count curve.

---

## 7. Phase 5 — Reports (`report.py`, `plots.py`)

Everything must be **presentable**: aligned terminal tables (use `rich` if installed, otherwise padded plain text), plus Markdown files, plus PNG plots (matplotlib; skip plots gracefully if it is missing).

### 7.1 After every run — `results/runs/<id>/report.md` + terminal print
1. Header: git SHA, dirty flag, bot hash, config, #games, duration.
2. **Overall**: W-L-D, decisive win rate, Wilson 95% CI, ELO-difference estimate.
3. **By opponent** (table): W-L-D, win %, Wilson CI, avg rounds, avg margin (my longest − theirs).
4. **By map** (table): same columns; **By side A/B**.
5. **Match statistics block** (candidate vs average opponent; also per opponent on request): longest dragon at end, dragons at end, pearls collected / total (%), deaths (suicide / attacked / other / unclassified), no-legal-move turns, stuck-loop turns, loop count + mean/max loop length, CPU p99/max.
6. **Gate results**: PASS/FAIL/WARN per gate with the numbers (Section 9).
7. Unclassified death causes seen (so I can extend rules).
8. Plots in `plots/`: length curve (longest dragon vs round, mean with p25–p75 band, both teams), unit-count curve (same), per-map win-rate bars with Wilson error bars, CPU p99 per opponent.

### 7.2 Per-bot cumulative report — `results/bots/<bot-id>/report.md` + `stats.json` + `plots/`
**Every bot that ever appears in a game gets one** (baselines, opponents, history, and the active bot). Built by aggregating **all** `results/runs/*/games.jsonl` (idempotent; `bc bot-report [--bot X|--all]` rebuilds it from scratch). Contents:
- Identity: group, folder, latest `bot_hash`, number of games, date range, current ELO.
- **Per-map table**: games, wins, losses, draws, **win % per map** (+ Wilson CI), sorted by map.
- **Per-opponent table** (head-to-head record).
- **Average rate per game** of each metric (mean ± std): longest dragon at end, dragons at end, pearls collected, pearls collected / total %, deaths, suicide deaths, deliberate-attack deaths (kills suffered), **kills delivered** (opponent deaths classified `deliberate_attack` credited to this bot if derivable), no-legal-move turns, stuck-loop turns, loops per game, mean/max loop length, CPU p99/max.
- **Average length curve and unit-count curve across all its games** (overall and, optionally, per map).
- For the active bot only: a **history timeline** table keyed by `bot_hash` (win rate vs the standard gauntlet per version) so I can see progress across versions.

---

## 8. Phase 6 — ELO leaderboard (`elo.py`)

- Source of truth: all games in `results/runs/*/games.jsonl` (decisive and draws; draws = ½ point; errors excluded).
- Rating identity: archived bots by folder id; the active bot by `active@<bot_hash>` (so each code version has its own rating; the leaderboard marks the newest as `(current)`).
- Algorithm: fit ratings by **maximum-likelihood Bradley–Terry** (iterate to convergence), converted to the ELO scale (`400/ln(10)` factor), anchored so the **mean rating of baselines = 1000** (document the anchor). Also implement the classic online ELO (K=32, game order = timestamp) as a cross-check and print both. Report games played per bot and a ±95% estimate (e.g. from bootstrap over games, 200 resamples — fast enough) so I know how trustworthy each rating is.
- Output `results/elo_leaderboard.json` and `results/elo_leaderboard.md` (+ terminal table): rank, bot id, group, rating, ±, games, W-L-D, hash.
- Add `bc roundrobin --games N [--selectors ...]`: plays all bots in a group against each other so the comparison graph is **connected** (ratings are meaningless between bots that never met through a chain of opponents). The leaderboard prints a warning if the graph is disconnected.
- Update the leaderboard automatically at the end of every `uv run bctool run` (cheap), and `uv run bctool elo` rebuilds it manually.

---

## 9. Phase 7 — Hard gates & the four tiers (`gates.py`, `cli.py`)

**Tiers** (each is `uv run bctool <tier>` and a `make` target; all run with `--sandbox`):
| Tier | Command | Purpose | Size |
|---|---|---|---|
| 0 Smoke | `uv run bctool smoke` / `make smoke` | compiles, runs, within CPU budget | 1 map, 2 games (both sides) vs one baseline, seconds |
| 1 Quick | `uv run bctool quick` / `make quick` | obvious better/worse | ~40 games, parallel |
| 2 Full | `uv run bctool full --games N` / `make full GAMES=400` | full gauntlet | **N is a parameter** (default 400); all opponents × all train maps, seeds cycled, sides balanced |
| 3 Ladder | manual | real-world check | `bc presubmit` prints the checklist and gate status; never uploads anything |

**Make wrapper** must support `make full GAMES=800`, `make quick`, `make smoke`, `make leaderboard`, `make report`, `make snapshot NAME=v1.1`, `make presubmit`, `make test`. Equivalent VS Code tasks in `.vscode/tasks.json` (with an input prompt for `GAMES`).

**Hard gates** (from `[gates]` in config; exit code non-zero on any FAIL so scripts can chain):
1. `max_errors`: zero errors/timeouts/crashes in the run.
2. CPU: candidate's **max** points-per-turn must be ≤ `max_cpu_points_M` (default 50M, i.e. half the judge's 100M); WARN above `warn_cpu_p99_M`. If the run was not sandboxed, the gate **FAILS with "CPU not measured"** (never pass silently).
3. Must-beat-baselines (full/presubmit only): Wilson lower bound > 0.5 vs each baseline.
4. No regression vs the newest `history` bot (full/presubmit only): decisive win rate ≥ threshold.
5. Unclassified death share warning.
`uv run bctool gates [--run latest]` re-evaluates gates on a stored run. Print a compact table with PASS/WARN/FAIL and the numbers.

---

## 10. Phase 8 — Snapshots & packaging (`snapshot.py`)

- `uv run bctool snapshot NAME` (e.g. `v1.1`): refuse if the working tree for `src/` is dirty unless `--force`; copy `src/` into `bots/history/NAME/` (including `bot.toml` and `helper.hpp`), write `bots/history/NAME/SNAPSHOT.json` (git sha, bot hash, date, optional `--note`), `git add` + commit (`snapshot NAME`), and `git tag NAME`. History bots are then automatically available as opponents (selector `history`).
- `bc package`: create `build/submission-<name>-<bot_hash>.zip` from `src/` (`main.cpp`, headers, `bot.toml`, `helper.hpp`), flat layout as the website expects (verify against the docs `/docs/submitting` and `/docs/cli`). Do not upload anything.
- `bc presubmit`: runs smoke → quick (or the last full run if recent and same `bot_hash`) → gates → prints a final **GO / NO-GO** with reasons and reminds me that a new submission's first 10 ranked battles move the rating faster, so don't waste them on an untested bot.

---

## 11. Phase 9 — Scenarios (lower priority; do last, keep small)

Read `/docs/map-files` and create three tiny maps in `scenarios/` (`portal.map`, `dead_end.map`, `head_on.map`) plus `scenarios/scenarios.toml` declaring, per scenario: map, opponent (a simple baseline), seeds, and an **assertion** (e.g. "candidate survives ≥ N rounds", "candidate collects ≥ K pearls by round R", "candidate does not die by self-collision"). `bc scenarios` runs them and prints PASS/FAIL. If the map format makes a scenario impossible, say so in `docs/DECISIONS.md` and skip it rather than faking it.

---

## 12. Phase 10 — Tests (`tests/`, pytest)

Use the fixtures saved in Phase 0 (real runner output / replay). Cover at least: result-line parsing (win/draw/error/points/deaths), Wilson interval (known values, n=0, edge p=0/1), ELO fit on a synthetic matrix with a known ordering, loop detector (synthetic cases above), death classifier (each rule + unclassified), bot hash (line-ending invariance, changes when a byte changes), registry/selectors, the game-distribution logic for `--games N` (coverage + side balance), resume logic, and the timeout/bytes regression. `make test` / `pytest -q` must pass before each commit.

---

## 13. Phase 11 — Documentation you must write (`README.md`, `docs/USAGE.md`)

`docs/USAGE.md` must be a **step-by-step guide for me**, with copy-paste commands, covering:
1. **One-time setup**: run `uv sync`, check `unswbc` is on PATH, first `git` commit, run `uv run bctool list` and `uv run bctool smoke`.
2. **Daily loop** (my edit–test cycle): edit `src/` → `uv run bctool smoke` → `uv run bctool quick` → read `results/runs/<latest>/report.md` → decide keep/revert via git → when a change is good: `git commit`.
3. **Before submitting**: `uv run bctool full --games 400` → `uv run bctool presubmit` → `uv run bctool package` → upload manually → `uv run bctool snapshot vX.Y`.
4. **How to read every table/plot/metric** (what each number means, what a "bad" value looks like, e.g. high `suicide` deaths early ⇒ tactical bug; large `stuck_loop_turns` ⇒ exploration bug; CPU max near the limit ⇒ danger; wide Wilson interval ⇒ not enough games, run more).
5. **How the statistics work**: Wilson interval in plain words, why ~100 games gives ±10 points and ~400 gives ±5, why draws are separated, how ELO is anchored and why `roundrobin` is needed.
6. **How reproducibility works**: SHA, `dirty`, `bot_hash`, seeds, `--resume`, re-running a specific game (`uv run bctool replay-cmd <run> <game-index>` prints the exact `unswbc` command to reproduce it — implement this tiny helper).
7. **How to add** a new baseline/opponent, a new map, a new death rule, a new metric.
8. **Troubleshooting** (runner not found, bot dir not accepted, replay parse failure, Windows path issues, "CPU not measured").
9. A **cheat-sheet table** of all commands.

`README.md`: short overview, the tree, links to `docs/USAGE.md`.

---

## 14. Definition of done (verify and report each)

- [ ] Repo restructured per Section 3.2; **before/after regression check** on the split bot gives identical results for the same seeds (state which seeds/maps).
- [ ] `git` initialised, `.gitignore`/`.gitattributes` in place, baseline commit + tag, every run stamped with SHA + dirty + bot hash.
- [ ] `uv run bctool smoke`, `quick`, `full --games 40` all work end-to-end (use a small number for the acceptance run) and produce `results/runs/<id>/{games.jsonl,summary.json,report.md,plots/}`.
- [ ] Parallel runner, incremental JSONL, `--resume`, timeout bug fixed.
- [ ] Live per-game progress line with Wilson interval (per opponent and overall).
- [ ] Per-run report with overall / by-opponent / by-map / by-side tables, match statistics block, gates, plots (length curve, unit-count curve).
- [ ] Per-bot cumulative reports in `results/bots/<id>/` for every bot (per-map win %, head-to-head, average-per-game metrics, average length and unit curves).
- [ ] ELO leaderboard (`results/elo_leaderboard.json/.md`), roundrobin command, connectivity warning.
- [ ] Hard gates + exit codes + `presubmit` GO/NO-GO.
- [ ] `snapshot` and `package` commands.
- [ ] Metric Availability Table honest about anything not derivable; no fabricated numbers.
- [ ] Tests pass (`pytest -q`).
- [ ] `docs/USAGE.md`, `docs/DECISIONS.md`, `docs/DISCOVERY.md`, `README.md` written.
- [ ] SPRT **not** implemented.

## 15. Your final message to me

When finished, reply with: (1) what you built, (2) the final tree, (3) the metrics that are **not** available or only approximated, and why, (4) any deviations from this prompt, (5) the exact first five commands I should run, in order, and (6) known limitations / next suggested steps. Keep it concise and concrete.
