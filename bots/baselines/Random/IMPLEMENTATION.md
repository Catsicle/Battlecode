# Bot_1 Implementation

## Purpose

`Bot_1` is a C++ Battlecode bot implementing the requested local 7x7 bitboard representation, Manhattan-distance utility function, depth-limited move search, and split fallback.

The bot is isolated from the existing `cpp`, `Utility`, and `mybot` projects. It includes a local copy of the protocol helper supplied with this workspace.

## Files

- `main.cpp`: bot implementation.
- `bot.toml`: C++ project configuration.
- `IMPLEMENTATION.md`: this technical handoff.
- `agents.md`: short instructions for future coding agents.

## Board representation

The visible board is exactly 7x7, so each visible tile maps to one bit in a `uint64_t`. Bitboards are maintained for:

- live pearls,
- future/dead pearl tiles (`pearl_time >= 0` and no current pearl),
- tiles incident to kelp edges,
- own body,
- allied bodies and heads,
- enemy bodies and heads,
- portal endpoint tiles,
- obstacles.

Kelp and portals are edges in the game protocol. Direction-specific kelp masks are used for move legality. Portal endpoints are paired by portal ID.

## Visibility and unknown cells

Only the current 49-tile vision window is represented. A normal move whose destination is outside the window terminates that search branch. Unknown space has no additional utility penalty, effectively using the configured initial unknown penalty of zero.

For portals, the bot only simulates a teleport when the paired portal endpoint and the emergence tile are both in the current 7x7 window. Otherwise the branch is illegal for search purposes and is not selected.

## Snake simulation

The visible own body is reconstructed by walking spatially behind the head and each subsequent segment using its facing direction. A visited mask prevents duplicate cells and cycles. The simulated state stores an ordered head-to-tail array, a body bitboard, the head cell, direction, and current size.

Each simulated move:

1. Checks the directional kelp edge.
2. Resolves a visible portal, if present.
3. Rejects occupied body and obstacle destinations.
4. Removes the tail unless the destination contains a live pearl.
5. Shifts the body order using bit masks plus a fixed array.
6. Adds the new head and updates direction.

No sprint sequences are searched. Allies and enemies remain fixed during the search.

## Utility function

Pearls are not utility targets. They remain in the board and movement simulation only so that moving onto a live pearl correctly increases the simulated dragon length.

The evaluator begins with the length term:

`state.length * LENGTH`

The default macro is `#define LENGTH 1.0`. Change that macro to tune the importance of length.

It then sums reciprocal Manhattan-distance contributions from the simulated head to every non-pearl target:

- `KELP / distance`
- `ENEMY_HEAD / distance`
- `ENEMY_BODY / distance`
- `PORTAL / distance`
- `ALLY_HEAD / distance`
- `ALLY_BODY / distance`

The own-body term is:

`popcount(own body inside the head's 3x3 neighborhood) / 8 * OWN_BODY`

Zero-distance reciprocal terms are skipped. Pearl masks are intentionally absent from utility scoring.

## Search

The search is a depth-12 maximizing tree. Each node branches over north, east, south, and west. Illegal moves are excluded. A node with no legal move receives negative infinity. Equal root scores are resolved randomly across the tied legal moves.

To stay below the judge's per-turn CPU budget, each root decision has a fixed 5,000-node budget. If the budget is reached before depth 12, the current state is evaluated as a frontier leaf. Thus depth 12 remains the maximum search depth, while pathological open positions cannot expand all `4^12` nodes.

The root evaluates all currently legal moves and chooses the highest-scoring result. A move remains a valid root candidate even when its future search reaches a terminal `NEG_SCORE` state. If no root move is legal, if the selected move enters a portal, or if the current controller-reported length exceeds the configurable hard limit, the bot attempts a split with child size `length - 2`.

When the selected move enters a portal, the split leaves a 2-segment parent at the portal entrance. That small dragon enters the portal on its next turn. The hard limit is:

`max(min(map_width, map_height) / K, M)`

The current defaults are `#define K 1.5` and `#define M 5`. `K` controls the divisor applied to the smaller map dimension, and `M` is the minimum hard-limit length regardless of map size. The divided value is truncated to an integer before applying the minimum.

The helper always splits from the tail. Passing `length - 2` therefore leaves the parent with 2 head-side segments and creates a child containing the rear `length - 2` segments. The engine determines the resulting orientations. An unresolved portal pair is treated as a terminal search branch rather than as an ordinary adjacent move.

## Protocol behavior

Each turn produces exactly one `MOVE` or `SPLIT` action and then calls `unswbc::end_turn()`. No logging, sonar, or sprinting is used. Randomness is used only to break ties between root moves with equal utility scores.

## Important implementation assumptions

- A future pearl tile is identified by `get_pearl_time() >= 0` when no live pearl is present. A countdown of `-1` is excluded because that tile never spawns.
- Kelp is represented by its incident tile(s) for utility distance and by directional edge masks for legality.
- An obstacle mask includes visible dragon occupancy and kelp-incident tiles. The direct legality check uses the directional kelp mask and body occupancy because kelp is fundamentally an edge obstacle.
- Portal destination geometry uses the opposite side of the paired portal edge, matching the documented portal behavior.
- `BOT_DEBUG` adds replay logs for body reconstruction, raw versus simulated legality, candidate scores, and the selected move; it is disabled by default.