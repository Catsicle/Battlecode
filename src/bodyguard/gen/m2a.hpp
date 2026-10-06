#pragma once
#include <array>
#include <algorithm>
#include <vector>

#include "bodyguard/core/constants.hpp"
#include "bodyguard/core/types.hpp"

namespace bodyguard {

using bot::bodyguard::core::Cell;
using bot::bodyguard::core::Grid;
using bot::bodyguard::core::Cycle;
using bot::bodyguard::core::CycleCandidate;
using bot::bodyguard::core::MAX_CYCLE_LEN;
using bot::bodyguard::core::PocketRegion;
using CandidateBuffer = std::vector<CycleCandidate>;

// M2a rectangle ring generator — full spec from docs/PROMPT.MD.
// Uses runE / runS, buildRuns double-backward loop, emit, interior-known, ranking.

inline void buildRuns(const Grid& g,
                      std::array<std::array<int, MAX_W>, MAX_H>& runE,
                      std::array<std::array<int, MAX_W>, MAX_H>& runS) {
    for (int y = 0; y < g.H; ++y) {
        int r = 0;
        for (int k = 2 * g.W - 1; k >= 0; --k) {
            int x = (k >= g.W) ? (k - g.W) : k;
            int cell = y * g.W + x;
            r = (!g.usable(cell)) ? 0 : (1 + (g.plain(cell, bot::Dir('E')) ? r : 0));
            if (r > g.W) r = g.W;
            if (k < g.W) runE[y][x] = r;
        }
    }
    for (int x = 0; x < g.W; ++x) {
        int r = 0;
        for (int k = 2 * g.H - 1; k >= 0; --k) {
            int y = (k >= g.H) ? (k - g.H) : k;
            int cell = y * g.W + x;
            r = (!g.usable(cell)) ? 0 : (1 + (g.plain(cell, bot::Dir('S')) ? r : 0));
            if (r > g.H) r = g.H;
            if (k < g.H) runS[y][x] = r;
        }
    }
}

inline void emitBoxes(const Grid& g, int cap,
                      const std::array<std::array<int, MAX_W>, MAX_H>& runE,
                      const std::array<std::array<int, MAX_W>, MAX_H>& runS,
                      std::vector<std::array<int, 4>>& out) {
    for (int y0 = 0; y0 < g.H; ++y0) {
        for (int x0 = 0; x0 < g.W; ++x0) {
            int mw = std::min(runE[y0][x0], g.W - 1);
            int mh = std::min(runS[y0][x0], g.H - 1);
            for (int h = 4; h <= mh; ++h) {
                int yb = y0 + h - 1; if (yb >= g.H) yb -= g.H;
                for (int w = 4; w <= mw; ++w) {
                    if (2 * (w + h) - 4 > cap) break;
                    if (runE[yb][x0] < w) continue;
                    int xr = x0 + w - 1; if (xr >= g.W) xr -= g.W;
                    if (runS[y0][xr] < h) continue;
                    out.push_back({x0, y0, w, h});
                }
            }
        }
    }
}

inline bool interiorUnseen(const Grid& g, int x0, int y0, int w, int h) {
    int unseen = 0;
    for (int y = y0 + 1; y < y0 + h - 1 && y < g.H; ++y) {
        for (int x = x0 + 1; x < x0 + w - 1 && x < g.W; ++x) {
            if (x >= 0 && y >= 0) {
                int cell = y * g.W + x;
                if (!g.usable(cell)) unseen++;
            }
        }
    }
    return unseen > 0;
}

inline PocketRegion get_pocket_region(const Grid& grid, const Cycle& cycle, Cell pocket) {
    PocketRegion region{};
    if (cycle.len <= 0) return region;
    int min_x = grid.W, max_x = -1, min_y = grid.H, max_y = -1;
    for (int i = 0; i < cycle.len; ++i) {
        int cx = cycle.cells[i] % grid.W;
        int cy = cycle.cells[i] / grid.W;
        if (cx < min_x) min_x = cx;
        if (cx > max_x) max_x = cx;
        if (cy < min_y) min_y = cy;
        if (cy > max_y) max_y = cy;
    }
    for (int y = min_y + 1; y < max_y && y < grid.H; ++y) {
        for (int x = min_x + 1; x < max_x && x < grid.W; ++x) {
            int idx = y * grid.W + x;
            if (idx >= 0 && idx < MAX_CELLS) {
                region.cells.set(idx);
                region.size++;
                if (x == 0 || x == grid.W - 1 || y == 0 || y == grid.H - 1) {
                    region.touches_exterior = true;
                }
            }
        }
    }
    return region;
}

inline int rectSum(const Grid& grid, int x0, int y0, int w, int h) {
    int sum = 0;
    for (int y = y0; y < y0 + h && y < grid.H; ++y) {
        for (int x = x0; x < x0 + w && x < grid.W; ++x) {
            if (x >= 0 && y >= 0) {
                int cell = y * grid.W + x;
                sum += (!grid.usable(cell)) ? 1 : 0;
            }
        }
    }
    return sum;
}

inline int generate_rectangles(const Grid& grid, Cell pocket, int c_cap, CandidateBuffer& out) {
    int count = 0;
    std::array<std::array<int, MAX_W>, MAX_H> runE{};
    std::array<std::array<int, MAX_W>, MAX_H> runS{};
    buildRuns(grid, runE, runS);
    for (int y0 = 0; y0 < grid.H; ++y0) {
        for (int x0 = 0; x0 < grid.W; ++x0) {
            int mw = std::min(runE[y0][x0], grid.W - 1);
            int mh = std::min(runS[y0][x0], grid.H - 1);
            for (int h = 4; h <= mh; ++h) {
                int yb = y0 + h - 1; if (yb >= grid.H) yb -= grid.H;
                for (int w = 4; w <= mw; ++w) {
                    if (2 * (w + h) - 4 > c_cap) break;
                    if (runE[yb][x0] < w) continue;
                    int xr = x0 + w - 1; if (xr >= grid.W) xr -= grid.W;
                    if (runS[y0][xr] < h) continue;

                    std::array<Cell, MAX_CYCLE_LEN> cells{};
                    cells[0] = y0 * grid.W + x0;
                    cells[1] = y0 * grid.W + (x0 + w - 1);
                    cells[2] = (y0 + h - 1) * grid.W + (x0 + w - 1);
                    cells[3] = (y0 + h - 1) * grid.W + x0;
                    int len = 4;

                    bool pocket_in_cycle = false;
                    for (int i = 0; i < len; ++i) {
                        if (cells[i] == pocket) {
                            pocket_in_cycle = true;
                            break;
                        }
                    }
                    if (pocket_in_cycle) continue;

                    int interior_unseen = rectSum(grid, x0 + 1, y0 + 1, w - 2, h - 2);
                    if (interior_unseen <= 0) continue;

                    Cycle candidate_cycle{};
                    for (int i = 0; i < len; ++i) candidate_cycle.cells[i] = cells[i];
                    candidate_cycle.len = len;
                    PocketRegion region = get_pocket_region(grid, candidate_cycle, pocket);
                    if (region.touches_exterior) continue;

                    CycleCandidate candidate{};
                    candidate.cycle = candidate_cycle;
                    candidate.pocket = pocket;
                    candidate.door_index = -1;
                    candidate.score = c_cap - (2 * (w + h) - 4);
                    out.push_back(candidate);
                    count++;
                }
            }
        }
    }
    return count;
}

} // namespace bodyguard
