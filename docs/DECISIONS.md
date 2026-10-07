# Decisions

## 2026-10-01: active bot directory

`src/` is the active bot because the runner expects a directory containing
`bot.toml` and a main source file. `helper.hpp` is moved unchanged.

## 2026-10-01: defer C++ split

The active bot now uses multiple C++ translation units listed explicitly in
`src/bot.toml`. The native CMake target and the runner both compile the modular
layout successfully, so the earlier header-only split constraint is no longer
applicable to `src/`.

## 2026-10-05: conservative occupancy age

Observed occupancy retains its last-observed round rather than being cleared
after two rounds. Clearing stale cells made the current fallback navigator treat
uncertain locations as free and caused both fixed-seed smoke games to lose.
The age is retained for a future safety layer to interpret conservatively.

## 2026-10-05: visible portal exits

`portal_exit_known` is set only when both endpoints with the same portal ID are
visible during one map update. A single observed endpoint is insufficient to
select a portal route because its exit geometry remains unknown.

## 2026-10-05: modular build validation

The active source set was validated with CMake and `unswbc 1.2.9`. Python tests
and two sandbox smoke games on the arena map also passed after the memory
changes.

## 2026-10-01: binary replays

Existing replays are binary and the matching parser is unavailable. Replay-derived
fields are therefore null with a reason rather than guessed.

## 2026-10-06: bodyguard / rectangular ring integration

Cycle detection (`CycleDetector`), ring generation (`rect_ring`), and `BodyguardRole` are integrated. Normal role (`StubRole`) runs until a period-2..16 trajectory repeats; then `BodyguardRole` takes over persistently. Direct movement (`NavMode::Direct`) bypasses the planner for predetermined perimeter steps. All new structures are fixed-size and deterministic; the large `rect::Snapshot` has static storage duration.

## 2026-10-01: bctool and uv

The package is named `bctool` instead of `bc` because `bc` conflicts with the
Unix calculator command. Project installation and execution use `uv`; editable
installation makes `bctool` importable from the repository and removes the need
for `PYTHONPATH` or `sys.path` hacks in wrapper scripts.
