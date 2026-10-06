# Navigation changelog

## Safety-stack pass

- Added persistent ordered-body tracking and reconciliation.
- Replaced head-only pearl bookkeeping with `MoveRecord`.
- Added occupancy freshness and owner metadata.
- Added shared normal/kelp/portal transition resolution.
- Added stamp-based simulation with undo.
- Added L1, L3, and L4 validation and executor retries.
- Registered all navigation translation units in `src/bot.toml`.

## Known follow-up

- The optional Dial queue remains disabled.
- Sonar payload decoding remains a placeholder by design.
- Search still uses the existing weighted priority queue; enabling a Dial
  implementation requires a separate performance-validation pass.
