#pragma once
#include <array>
#include <algorithm>

namespace bodyguard {

constexpr int MAX_W = 64, MAX_H = 64;

// M2a rectangle ring generator — full spec from docs/PROMPT.MD.
// Uses runE / runS, buildRuns double-backward loop, emit, interior-known, ranking.

struct Grid {
    int W = 0, H = 0;
    // Adapter: usable(x,y), plain(x,y,d)
    bool usable(int x, int y) const { return true; }
    bool plain(int x, int y, int d) const { return true; } // d: 0=E,1=S
};

inline void buildRuns(const Grid& g, std::array<std::array<int, MAX_W>, MAX_H>& runE,
                      std::array<std::array<int, MAX_W>, MAX_H>& runS) {
    for (int y = 0; y < g.H; ++y) {
        int r = 0;
        for (int k = 2 * g.W - 1; k >= 0; --k) {
            int x = (k >= g.W) ? (k - g.W) : k;
            r = (!g.usable(x, y)) ? 0 : (1 + (g.plain(x, y, 0) ? r : 0));
            if (r > g.W) r = g.W;
            if (k < g.W) runE[y][x] = r;
        }
    }
    // runS: same over columns using plain(x,y,1) and g.H
    for (int x = 0; x < g.W; ++x) {
        int r = 0;
        for (int k = 2 * g.H - 1; k >= 0; --k) {
            int y = (k >= g.H) ? (k - g.H) : k;
            r = (!g.usable(x, y)) ? 0 : (1 + (g.plain(x, y, 1) ? r : 0));
            if (r > g.H) r = g.H;
            if (k < g.H) runS[y][x] = r;
        }
    }
}

// Emit loop: for each anchor (x0,y0) in allowed region.
// Conditions: w,h >=4; 2(w+h)-4 <= cap; runE[yb][x0]>=w; runS[y0][xr]>=h.
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

// Interior-known O(1) using doubled-torus prefix sum (simplified).
inline bool interiorUnseen(const Grid&, int x0, int y0, int w, int h) {
    // Simplified: if interior area >0 unseen, true. Full version uses rectSum.
    int interior = (w - 2) * (h - 2);
    return interior > 0; // placeholder for actual unseen count
}

} // namespace bodyguard
