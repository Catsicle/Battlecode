# Memory Usage (`memory.hpp`)

## Directions (`dir`)
- `0` = North (`N`)
- `1` = East (`E`)
- `2` = South (`S`)
- `3` = West (`W`)

## Initialization
```cpp
bot::Memory memory;
memory.initialize(width, height);
```

## Update (`update`)
```cpp
memory.update(controller, round);
```
Sets `my_team` from `controller.get_team()`, updates `seen`, `occupancy`, `pearl`, `kelp`, `portal_id`, pairs portals (`portal_exit_known`).

## Queries

### `get_edge(memory, x, y, dir)` → `pair<optional<int>, optional<int>>`
- First value:
  - `NULL` (`nullopt`) = out of bounds
  - `-1` = unknown edge
  - `0` = empty
  - `1` = kelp
  - `2` = portal (`second` = portal id)
- Second value:
  - `0` for non-portal / unknown / out of bounds
  - `portal_id` when portal (`first == 2`)

### `get_cell(memory, x, y)` → `pair<optional<int>, optional<int>>`
- First value (occupancy):
  - `NULL` = out of bounds
  - `-1` = not visited
  - `0` = empty
  - `1` = own body
  - `2` = own head
  - `3` = ally
  - `4` = ally head
  - `5` = enemy
  - `6` = enemy head
- Second value (pearl):
  - `NULL` = out of bounds
  - `0` = no pearl
  - `1` = pearl present

Team classification uses `memory.my_team` (set in `update`).
