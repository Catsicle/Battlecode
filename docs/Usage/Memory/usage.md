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

## Moved query functions (normal, non-inline)

### `mapview_width(const Memory&)` → `int`
- Input: `Memory` adapter (`memory`)
- Output: `memory.width`

### `mapview_height(const Memory&)` → `int`
- Input: `Memory`
- Output: `memory.height`

### `mapview_cells(const Memory&)` → `int`
- Input: `Memory`
- Output: `memory.cells` (`width * height`)

### `mapview_valid(const Memory&, int cell)` → `bool`
- Input: `Memory`, `cell` index
- Output: `true` if `cell >= 0 && cell < memory.cells`

### `query_known(const Memory&, int cell)` → `bool`
- Input: `Memory`, `cell`
- Output: `memory.known(cell)`

### `query_frontier(const Memory&, int cell)` → `bool`
- Input: `Memory`, `cell`
- Output: `memory.is_frontier(cell)`

### `query_seen(const Memory&, int cell)` → `bool`
- Input: `Memory`, `cell`
- Output: `valid(cell) ? memory.seen[cell] : false`

### `query_edge_known(const Memory&, int cell, int direction)` → `bool`
- Input: `Memory`, `cell`, `direction` (`0=N,1=E,2=S,3=W`)
- Output: `valid(cell) ? memory.edge_known(cell, direction) : false`

### `query_last_seen_round(const Memory&, int cell)` → `int`
- Input: `Memory`, `cell`
- Output: `valid(cell) ? memory.last_seen_round[cell] : -1`

### `query_has_pearl(const Memory&, int cell)` → `bool`
- Input: `Memory`, `cell`
- Output: `valid(cell) ? memory.pearl[cell] : false`

### `query_pearl_spawn_round(const Memory&, int cell)` → `int`
- Input: `Memory`, `cell`
- Output: `valid(cell) ? memory.pearl_spawn_round[cell] : std::numeric_limits<int>::max()`

### `query_never_spawns(const Memory&, int cell)` → `bool`
- Input: `Memory`, `cell`
- Output: `valid(cell) ? memory.never_spawns[cell] : false`

All declared in `queries.hpp`, defined in `queries.cpp`; call them instead of `map_view.hpp` members.
