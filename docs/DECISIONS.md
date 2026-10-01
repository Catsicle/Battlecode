# Decisions

## 2026-10-01: active bot directory

`src/` is the active bot because the runner expects a directory containing
`bot.toml` and a main source file. `helper.hpp` is moved unchanged.

## 2026-10-01: defer C++ split

The runner is unavailable, so multiple-source compilation could not be tested.
`main.cpp` remains unchanged; a header-only split is deferred until a real build
proves the safe translation-unit boundary.

## 2026-10-01: binary replays

Existing replays are binary and the matching parser is unavailable. Replay-derived
fields are therefore null with a reason rather than guessed.

## 2026-10-01: bctool and uv

The package is named `bctool` instead of `bc` because `bc` conflicts with the
Unix calculator command. Project installation and execution use `uv`; editable
installation makes `bctool` importable from the repository and removes the need
for `PYTHONPATH` or `sys.path` hacks in wrapper scripts.
