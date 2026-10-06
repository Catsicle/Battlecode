#pragma once

constexpr int MAX_W = 64;
constexpr int MAX_H = 64;
constexpr int MAX_CELLS = MAX_W * MAX_H;

constexpr int LOCAL_W = 32;
constexpr int LOCAL_H = 32;

constexpr int MAX_CYCLE_LEN = 64; // based on confirmed c_cap
constexpr int MAX_DOUBLE_STATES = 2 * LOCAL_W * LOCAL_H;
