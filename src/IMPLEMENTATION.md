# Bodyguard Integration Note

Implemented `queen_beacon_sonar` (sonar ID 0100) with encoding (`comm_function.cpp`), decoding (`decode_sonar`), emission (`use_sonar.cpp`), and inbox processing (`comms.cpp`). Added `maybe_emit_queen_beacon_sonar()` following `maybe_emit_enemy_queen_sonar()` style. Added debug statements to bodyguard.cpp (guard_state, split, intercept, orbit, beacon emission/usage) and to beacon functions (encode/decode/emit). `selector.cpp` unchanged. Full compilation verified (`g++ -std=c++20 -O2`).
