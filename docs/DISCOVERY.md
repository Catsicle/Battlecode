# Discovery

Date: 2026-10-01

## Environment

- The active branch is `jha`, based on the existing initial commit.
- `unswbc` is not installed or available on `PATH`; fresh game fixtures and help output therefore could not be collected.
- Official CLI documentation says `unswbc run` accepts bot project directories and rebuilds each directory before a match. No staging workaround is required by the documented interface.
- Existing replay files are binary containers with embedded text markers such as `MAP`, not plain JSON. Their schema cannot be safely inferred without the matching toolkit parser.

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
| longest dragon, dragons at end | replay | unavailable until parser is verified |
| pearls collected/total | replay | unavailable; spawn history is required |
| death categories | output, optional replay | rules available; attack disambiguation is conservative |
| no-legal-move turns | replay | unavailable until per-turn state is verified |
| stuck loops | replay | unavailable until head positions are verified |
| length/unit curves | replay | unavailable until replay schema is verified |
