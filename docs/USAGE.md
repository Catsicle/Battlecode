# Usage

## Setup

```powershell
uv sync
uv run bctool list
uv run bctool smoke
```

Install `unswbc` separately and confirm `unswbc --help` works before running real
games. The CLI uses `--sandbox` by default for tiers so CPU gates are meaningful.

If `uv` is not found, install it from [docs.astral.sh/uv](https://docs.astral.sh/uv/).
If the environment is stale after changing dependencies, run `uv sync` again.

## Daily loop

Edit `src/`, run `uv run bctool smoke`, then `uv run bctool quick`. Read the report
in the printed run directory, then commit or discard the source change.

The game budget is distributed across every selected opponent, map, seed, and both
candidate sides. `--games N` chooses the smallest repeated schedule that covers
all of those dimensions; sides remain balanced.

## Reproducibility

Each row stores the Git SHA, dirty flag, bot hashes, Python version, runner version,
command, seed, and map. `--resume RUN_DIR` skips indexes already present in
`games.jsonl`. `uv run bctool replay-cmd RUN_DIR INDEX` prints the exact command.

## Reports and statistics

Reports separate wins, losses, draws, and errors. Wilson intervals describe the
uncertainty of decisive win rate; more games narrow the interval. Draws are not
silently counted as wins. The current ELO command is an online K=32 cross-check;
round-robin coverage is needed before comparing disconnected groups.

Replay-derived length, pearl, loop, and legal-move metrics are `null` with a reason
until the installed `unswbc` replay schema is verified. Unknown death causes remain
`unclassified`. The default attack rules are intentionally conservative because a
cause string alone cannot prove intent.

## Commands

| Command | Purpose |
| --- | --- |
| `uv run bctool list` | list registered bots |
| `uv run bctool run` | run a configurable gauntlet |
| `uv run bctool smoke` / `quick` / `full` | tiered sandbox runs |
| `uv run bctool report RUN_DIR` | rebuild a run report |
| `uv run bctool gates RUN_DIR` | evaluate stored gates |
| `uv run bctool elo` | rebuild the leaderboard |
| `uv run bctool snapshot NAME` | archive the active bot |
| `uv run bctool replay-cmd RUN INDEX` | print a reproduction command |

## Troubleshooting

- **uv not found:** install uv from [docs.astral.sh/uv](https://docs.astral.sh/uv/), then reopen the terminal.
- **Stale environment:** run `uv sync`.
- **Runner not found:** install or expose `unswbc` on `PATH`; it is invoked directly by the tool.
- **CPU not measured:** rerun with sandbox enabled; CPU gates intentionally fail for unsandboxed runs.

