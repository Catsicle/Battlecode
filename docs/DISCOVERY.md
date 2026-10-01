# Discovery

Date: 2026-10-01

## Environment

- The active branch is `jha`, based on the existing initial commit.
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
