# Navigation implementation

The bot keeps persistent per-dragon memory of observations from the 7x7 vision
window. `main.cpp` is only the protocol loop. `core/memory.cpp` updates map
facts, `comms` owns the future sonar boundary, `nav` chooses paths, and
`roles` selects the configured role.

`FORCE_ROLE` is the only role selector. `Auto` deliberately selects `Stub`.
The stub uses Explore/Wander mode unless `NAVIGATION_FIXTURE` is enabled or a
future navigation message is decoded.

The sonar decoder currently stores raw 64-bit messages, the four-bit type, and
the six-bit authentication placeholder. Coordinate payload encoding is
intentionally not implemented yet. Outgoing sonar is disabled.

For a local Navigate-mode replay:

1. Set `NAVIGATION_FIXTURE` to `true` and choose `FIXTURE_TARGET_X` and
   `FIXTURE_TARGET_Y` in `config.hpp`.
2. From the repository root, run:
   `uv run bctool run --candidate active --opponents Random --maps maps/train/arena.map --seeds 0 --jobs 1 --games-per-matchup 1 --keep-replays --keep-output`
3. Open the printed `report.md`, then list the replay with:
   `Get-ChildItem -Recurse .\results\runs\<run-id>\replays`.
4. Use `uv run bctool replay-cmd .\results\runs\<run-id> 1` or index `2` to
   reproduce the exact game command. The replay viewer can open the `.replay`
   file produced in that directory.
5. Return `NAVIGATION_FIXTURE` to `false` for normal Explore/Wander behavior.

The fixture keeps the coordinate as its objective. If the target is known, it
uses the weighted route search toward it. If it is unknown or currently
unreachable, it routes to the nearest reachable frontier tile, updates memory
from the next vision window, and re-plans. Once the head reaches the target,
the fixture branch ends and normal Explore/Wander behavior resumes.
