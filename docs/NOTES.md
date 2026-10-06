# Navigation verification notes

- `helper.hpp` was not modified.
- Sonar target payload decoding is intentionally unchanged and remains out of
  scope.
- Portal transitions require two observed endpoints and a visible, non-kelp
  exit edge. This conservative behavior avoids planning through unknown portal
  geometry.
- Unknown portal destinations are terminal to Navigate planning.
- `OCC_STALE_ROUNDS` is three rounds, so old enemy occupancy is not treated as
  a permanent blocker.
- `USE_DIAL` and `NAV_STATS` remain disabled by default.
