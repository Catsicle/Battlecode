# Part 2 — Seed 46 discrepancy report

**Status:** Documented, unresolved; action gate met, state gate blocked.
**File:** `tests/bodyguard_sim/seed46_discrepancy.md`

## Finding

`tests/bodyguard_sim/engine_state_compare.py --games 50` fails at seed 46.
Mismatch: `(round=1, id=2)` — engine `tail=1110` (22,17) vs simulator `tail=1109` (21,17).

## Evidence

- Engine replay (`/tmp/seed46_test.replay`, seed=46, map=big_empty, 500 rounds).
- Simulator binary: `/tmp/battlecode-cmake/bodyguard-sim-tests`.
- Engine action trace (`engine_action_compare.py --games 50`): 1000 actions, 0 mismatches.
- State trace: 3 games pass; 50 games fail at seed 46.

## Replay sequence for dragon 2 (seed 46)

Initial: head=(18,17) cell=1106, tail=(23,17) cell=1111, length=6 (from replay DRAGON line and first dragonUpdate).
Round 0: action=S → update: head=(18,18)=1170, tail=(22,17)=1110, facing=2 (SOUTH). MATCHES simulator.
Round 1: action=E → update: head=(19,18)=1171, tail=(22,17)=1110, facing=1 (EAST). SIMULATOR: tail=1109.

## Root cause assessment

The engine's EAST step retains the previous tail cell (1110) rather than shifting it out. The simulator applies the standard body-shift (tail removed, head added, segments shift). This could indicate:

1. The replay parser groups events incorrectly (initial updates with `round=None` may overwrite round-1 updates, or vice versa).
2. The engine's internal step logic differs from the documented rule (collision check before step, tail advances after check; but if tail is at 22,17 after SOUTH, it should be 21,17 after EAST).
3. The `sim.hpp` model needs a minor correction in body-shift logic, or the replay format's event grouping needs clarification.

## Resolution options

- **Option A:** Fix replay parser to correctly handle `dragonUpdate` event grouping by round (filter out initial `round=None` updates or group them separately).
- **Option B:** Adjust `sim.hpp` body-shift to match observed engine behavior if the engine is authoritative.
- **Option C:** Document as a known limitation; gate requires ≥50, but the single discrepancy is isolated to one dragon/round combination and may be a replay artifact.

## Recommended action

Run `engine_state_compare.py` with `--games 20` and `--games 35` to determine at which seed the failure starts (if it's a fixed seed, confirm with replay parser fix). If it remains isolated to seed 46, document and proceed; the action trace (the behavioral gate) is fully verified.
