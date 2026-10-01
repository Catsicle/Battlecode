# Usage

## Setup

```powershell
python -m pip install -r requirements.txt
python -m bc list
python -m bc smoke
```

Install `unswbc` separately and confirm `unswbc --help` works before running real
games. The CLI uses `--sandbox` by default for tiers so CPU gates are meaningful.

## Daily loop

Edit `src/`, run `python -m bc smoke`, then `python -m bc quick`. Read the report
in the printed run directory, then commit or discard the source change.

The game budget is distributed across every selected opponent, map, seed, and both
candidate sides. `--games N` chooses the smallest repeated schedule that covers
all of those dimensions; sides remain balanced.

## Reproducibility

Each row stores the Git SHA, dirty flag, bot hashes, Python version, runner version,
command, seed, and map. `--resume RUN_DIR` skips indexes already present in
`games.jsonl`. `python -m bc replay-cmd RUN_DIR INDEX` prints the exact command.

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
| `python -m bc list` | list registered bots |
| `python -m bc run` | run a configurable gauntlet |
| `python -m bc smoke` / `quick` / `full` | tiered sandbox runs |
| `python -m bc report RUN_DIR` | rebuild a run report |
| `python -m bc gates RUN_DIR` | evaluate stored gates |
| `python -m bc elo` | rebuild the leaderboard |
| `python -m bc snapshot NAME` | archive the active bot |
| `python -m bc replay-cmd RUN INDEX` | print a reproduction command |
