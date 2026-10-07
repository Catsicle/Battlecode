# Implementation Changes

## Bodyguard / Rectangular Ring Integration (2026-10-06)

### Added

- `src/roles/cycle_detector.hpp` — fixed-size cycle detection (last 32 positions, periods 2–16).
- `src/roles/ring_path.hpp` — `RingPath`, `RingBox`, `StepDir`, clockwise rectangular ring generation, `rotate_path_to_start()`.
- `src/roles/bodyguard.hpp` / `bodyguard.cpp` — `BodyguardRole` with three states (`NeedRing`, `EnteringRing`, `Tracing`). Uses `rect_ring` APIs (`generate`, `validate`) with static `Snapshot` and exact direct-movement tracing.
- `NavMode::Direct` added to `core/types.hpp`.
- `PlanRequest` extended with `direct_direction` (`StepDir`) in `roles/role.hpp`.
- `step_dir_to_dir()` conversion in `ring_path.hpp`.

### Modified

- `src/roles/registry.cpp` — `ForcedRole::Bodyguard` maps to `bodyguard_role()`.
- `src/nav/executor.cpp` — `NavMode::Direct` dispatches directly to `controller.make_move()` without planner invocation; applies same memory/tracker updates.
- `src/main.cpp` — removed `cycle_detected` logic. Always selects `bodyguard_role()`. `BodyguardRole` manages `NeedRing` (tries adapter every turn) -> `EnteringRing` (navigate to `path.cells[0]`) -> `Tracing` (direct movement). Once `path.count > 0`, box/path persists without regeneration.

### Behavior

- `BodyguardRole` tries adapter (`fill_snapshot` + `query_top_boxes`) continuously in `NeedRing`. Once validated box produces `RingPath` (`count > 0`), enters `EnteringRing` (navigate to first cell), then `Tracing` (exact `Direct` movement). Divergence invalidates and rebuilds.
- Once detected, `BodyguardRole` takes over permanently for that game.
- `BodyguardRole` generates a validated rectangular ring through `rect_ring`, enters it via `NavMode::Navigate`, then traces the exact perimeter using `NavMode::Direct`.
- Divergence from the expected ring path invalidates the ring (`NeedRing`) and triggers regeneration.
- Large `bodyguard::rect::Snapshot` has static storage duration (`static` inside `build_ring`).

### Fixed-size / no-heap constraints preserved

- `CycleDetector`: `std::array<Vec2, 32>`.
- `RingPath`: `std::array<Vec2, 1024>` + `std::array<StepDir, 1024>`.
- No dynamic allocation in cycle detection, ring generation, or tracing.
