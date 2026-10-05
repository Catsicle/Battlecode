#pragma once

#include <cstdint>

enum class ForcedRole {
    Auto,
    Explorer,
    Killer,
    Feeder,
    Bodyguard,
    Queen,
    Stub,
};

// Change this one value for local role testing. Auto intentionally maps to Stub.
inline constexpr ForcedRole FORCE_ROLE = ForcedRole::Auto;

inline constexpr int LOG_LEVEL = 0;
inline constexpr bool ENABLE_SYMMETRY = false;
inline constexpr bool EXEMPT_SPAWNABLE = false;

inline constexpr int C_STEP = 4;
inline constexpr int C_PORTAL_EXTRA = 2;
inline constexpr int C_UNKNOWN = 6;
inline constexpr int C_PORTAL_UNKNOWN = 40;
inline constexpr int D0 = 120;
inline constexpr int P_MAX = 48;
inline constexpr int LAMBDA = 8;
inline constexpr int AGE_K = 150;
inline constexpr int HORIZON_T = 48;
inline constexpr int LOOKAHEAD_K = 3;
inline constexpr int B_PEARL = 120;
inline constexpr int B_SOON = 80;
inline constexpr int B_FRONT = 30;
inline constexpr int B_STALE = 20;

// Disabled by default. Set to true only for a local fixture/replay test.
inline constexpr bool NAVIGATION_FIXTURE = true;
inline constexpr int FIXTURE_TARGET_X = 0;
inline constexpr int FIXTURE_TARGET_Y = 0;
