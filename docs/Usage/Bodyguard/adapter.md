# Adapter Contract (`rect_adapter.hpp`)

The adapter bridges `Memory` (`src/core/memory.hpp`) and `bodyguard::rect::Snapshot` (`rect_ring.hpp`) using `rect_ring.hpp`'s existing direction convention (`0=E, 1=S, 2=W, 3=N`) with no coordinate remapping.

## Direct bit mapping (`RECT_D_TO_MEM_BIT`)

`rect_ring` `d` is mapped straight to Memory bit positions:

- `0` (East) -> Memory bit `1`
- `1` (South) -> Memory bit `2`
- `2` (West) -> Memory bit `3`
- `3` (North) -> Memory bit `0`

The adapter reads `memory.seen[]`, `memory.kelp[]`, `memory.known_edges[]`, and `memory.portal_id[]` directly through this mapping.

## Pipeline

`fill_snapshot(memory, current_round, Snapshot&)` performs:

1. `Snapshot::reset(W, H)`
2. Loop over `[0,W) x [0,H)`; read `memory.seen[cell]`, `memory.pearl[cell]`, `memory.never_spawns[cell]`, `memory.pearl_spawn_round[cell]`
3. Read edge kinds directly via `memory.known_edges[cell] >> mem_bit` / `memory.kelp[cell] >> mem_bit` / `memory.portal_id[cell][mem_bit]`
4. `Snapshot::finalize()`

No STLs, no heap, no direction-translation tables (only the single `RECT_D_TO_MEM_BIT` array).

## Query function (`query_top_boxes`)

`query_top_boxes(const Snapshot&, int px, int py, int cap, int k, Box* out) -> int`

- Returns up to `k` best `Box` structs (clamped to `<= 64`) for a pocket at `(px, py)`.
- Writes results into `out` array.
- Returns the actual number of boxes written (`<= k`).
- Uses `rect_ring::generate()` internally, preserving fixed-size / no-heap behavior.
