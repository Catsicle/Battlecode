# Agent Instructions for Bot_1

## Scope

Work only inside `Bot_1` unless the user explicitly requests changes elsewhere. Do not modify the existing `Utility`, `cpp`, `mybot`, or `killmode` bots as part of normal Bot_1 development.

## Architecture

- `main.cpp` contains the protocol loop, 7x7 bitboards, snake simulation, utility evaluation, and depth-limited search.
- `bot.toml` defines the C++ Battlecode project.
- `IMPLEMENTATION.md` is the source of truth for behavior and assumptions.
- `helper.hpp` is the local protocol-helper copy used by the bot.

## Behavioral contract

- Keep the board representation limited to the current 7x7 vision window.
- Preserve deterministic direction ordering.
- Do not add sprint search unless explicitly requested.
- Treat unknown destinations as search-terminal states.
- Simulate portals only when their paired endpoint and emergence tile are visible.
- Keep allies and enemies stationary in the tree.
- Preserve the emergency split fallback: call `do_split(length - 2)` so the parent keeps 2 head-side segments and the child receives the rear `length - 2` segments.

## Validation

From the repository root, compile with:

```sh
clang++ -std=c++20 -O2 -Wall -Wextra -pedantic Bot_1/main.cpp -o /tmp/battlecode-bot-1
```

Then use the local Battlecode runner or sandbox to test movement, kelp, visible portals, body collisions, pearl growth, no-move states, and splitting.

## Editing guidance

Keep the fixed-size structures and avoid per-node heap allocation. Any change to scoring constants, visibility assumptions, portal geometry, or split policy must also update `IMPLEMENTATION.md`.