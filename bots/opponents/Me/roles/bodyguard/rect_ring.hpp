// rect_ring.hpp -- rectangular ring generator + validator (header only, no heap, no STL).
//
// Conventions (single source of truth -- the adapter must convert INTO these):
//   direction index: 0 = East (+x), 1 = South (+y), 2 = West (-x), 3 = North (-y)
//   tile (x,y), 0 <= x < W, 0 <= y < H, torus wrap.
//   A "box" is the ring of a w x h rectangle whose top-left tile is (x0,y0).
//   Ring length c = 2*(w+h)-4 (always even). Interior = (w-2)*(h-2) tiles.
//
// Pipeline:  Snapshot::reset -> fill seen/edge/risk -> finalize -> generate -> validate.
#pragma once
#include <cstdint>

namespace bodyguard::rect {

constexpr int MAX_W    = 64;
constexpr int MAX_H    = 64;
constexpr int MAX_RING = 128;   // largest ring length supported (cap is clamped to this)
constexpr int MAX_DASH = 8;     // largest dash length supported

constexpr int DX[4] = {1, 0, -1, 0};
constexpr int DY[4] = {0, 1, 0, -1};

enum EdgeKind : uint8_t { EDGE_PLAIN = 0, EDGE_KELP = 1, EDGE_PORTAL = 2, EDGE_UNKNOWN = 3 };

// ---------------------------------------------------------------------------------------------
// Snapshot: flat copy of what the bot knows. Build once per replan. ~100 KB: make it static!
// ---------------------------------------------------------------------------------------------
struct Snapshot {
    int W = 0, H = 0;
    bool    seen[MAX_H][MAX_W];        // tile has been observed (not predicted)
    bool    risk[MAX_H][MAX_W];        // pearl present or about to spawn (dash must avoid it)
    uint8_t edge[MAX_H][MAX_W][4];     // EdgeKind of the edge leaving (x,y) in direction d
    int16_t runE[MAX_H][MAX_W];        // filled by finalize()
    int16_t runS[MAX_H][MAX_W];
    int32_t unseenPS[2 * MAX_H + 1][2 * MAX_W + 1];

    void reset(int w, int h) {
        W = w; H = h;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                seen[y][x] = false;
                risk[y][x] = false;
                for (int d = 0; d < 4; ++d) edge[y][x][d] = EDGE_UNKNOWN;
            }
    }

    // Call after all seen[]/edge[] are filled.
    void finalize() {
        // runE[y][x]: number of consecutive tiles starting at (x,y) going East, all seen, joined by
        // PLAIN edges (wrapping, capped at W). Two backward passes handle the wrap.
        for (int y = 0; y < H; ++y) {
            int r = 0;
            for (int k = 2 * W - 1; k >= 0; --k) {
                int x = (k >= W) ? k - W : k;
                r = !seen[y][x] ? 0 : 1 + (edge[y][x][0] == EDGE_PLAIN ? r : 0);
                if (r > W) r = W;
                if (k < W) runE[y][x] = static_cast<int16_t>(r);
            }
        }
        for (int x = 0; x < W; ++x) {
            int r = 0;
            for (int k = 2 * H - 1; k >= 0; --k) {
                int y = (k >= H) ? k - H : k;
                r = !seen[y][x] ? 0 : 1 + (edge[y][x][1] == EDGE_PLAIN ? r : 0);
                if (r > H) r = H;
                if (k < H) runS[y][x] = static_cast<int16_t>(r);
            }
        }
        // prefix sums of UNSEEN tiles on the doubled torus
        for (int y = 0; y <= 2 * H; ++y)
            for (int x = 0; x <= 2 * W; ++x) {
                if (y == 0 || x == 0) { unseenPS[y][x] = 0; continue; }
                int u = seen[(y - 1) % H][(x - 1) % W] ? 0 : 1;
                unseenPS[y][x] = u + unseenPS[y - 1][x] + unseenPS[y][x - 1] - unseenPS[y - 1][x - 1];
            }
    }

    // number of unseen tiles in the w x h rectangle with top-left (x,y) (wrapped). 0<=x<W,0<=y<H,w<=W,h<=H
    int unseenIn(int x, int y, int w, int h) const {
        return unseenPS[y + h][x + w] - unseenPS[y][x + w] - unseenPS[y + h][x] + unseenPS[y][x];
    }
};

// ---------------------------------------------------------------------------------------------
// Boxes
// ---------------------------------------------------------------------------------------------
struct Box {
    uint8_t  x0, y0, w, h, c;
    uint16_t area;       // interior tiles (w-2)*(h-2)
    bool     unseen;     // some interior tile has never been seen (candidate not committable yet)
};

// ranking (a strict total order, so results are deterministic): committable first, then larger
// interior, then shorter ring, then position, then width
inline bool better(const Box& a, const Box& b) {
    if (a.unseen != b.unseen) return !a.unseen;
    if (a.area != b.area) return a.area > b.area;
    if (a.c != b.c) return a.c < b.c;
    if (a.y0 != b.y0) return a.y0 < b.y0;
    if (a.x0 != b.x0) return a.x0 < b.x0;
    return a.w < b.w;            // transposed boxes (w x h vs h x w) tie on everything above
}

// Calls f(Box) for every rectangular ring with 4<=w<=W-1, 4<=h<=H-1, c<=cap whose ring tiles are all
// seen and joined by PLAIN edges. If px>=0, only boxes whose INTERIOR contains (px,py) are produced.
template <class F>
inline void forEachBox(const Snapshot& s, int px, int py, int cap, F&& f) {
    const int W = s.W, H = s.H;
    if (cap > MAX_RING) cap = MAX_RING;
    auto tryAnchor = [&](int x0, int y0, int w, int h) {
        int yb = y0 + h - 1; if (yb >= H) yb -= H;
        int xr = x0 + w - 1; if (xr >= W) xr -= W;
        if (s.runE[y0][x0] < w) return;        // top side
        if (s.runE[yb][x0] < w) return;        // bottom side
        if (s.runS[y0][x0] < h) return;        // left side
        if (s.runS[y0][xr] < h) return;        // right side
        int ix = x0 + 1; if (ix >= W) ix -= W;
        int iy = y0 + 1; if (iy >= H) iy -= H;
        Box b;
        b.x0 = static_cast<uint8_t>(x0); b.y0 = static_cast<uint8_t>(y0);
        b.w = static_cast<uint8_t>(w);   b.h = static_cast<uint8_t>(h);
        b.c = static_cast<uint8_t>(2 * (w + h) - 4);
        b.area = static_cast<uint16_t>((w - 2) * (h - 2));
        b.unseen = s.unseenIn(ix, iy, w - 2, h - 2) > 0;
        f(b);
    };
    for (int h = 4; h <= H - 1; ++h) {
        if (2 * (4 + h) - 4 > cap) break;
        for (int w = 4; w <= W - 1; ++w) {
            if (2 * (w + h) - 4 > cap) break;
            if (px < 0) {
                for (int y0 = 0; y0 < H; ++y0)
                    for (int x0 = 0; x0 < W; ++x0) tryAnchor(x0, y0, w, h);
            } else {
                for (int dy = 1; dy <= h - 2; ++dy)
                    for (int dx = 1; dx <= w - 2; ++dx)
                        tryAnchor((px - dx + W) % W, (py - dy + H) % H, w, h);
            }
        }
    }
}

// Best-N boxes, best first, no heap. Returns number written (<= N, <= 64).
// If `total` is given it receives the number of boxes enumerated (so callers can detect truncation).
inline int generate(const Snapshot& s, int px, int py, int cap, Box* out, int N, int* total = nullptr) {
    if (N > 64) N = 64;
    if (total) *total = 0;
    if (N <= 0) return 0;
    int n = 0;
    forEachBox(s, px, py, cap, [&](const Box& b) {
        if (total) ++*total;                       // counts every box enumerated, kept or not
        if (n == N && !better(b, out[N - 1])) return;
        int i = (n < N) ? n++ : N - 1;
        while (i > 0 && better(b, out[i - 1])) { out[i] = out[i - 1]; --i; }
        out[i] = b;
    });
    return n;
}

// Ring tiles in travel order (clockwise: east along the top, south, west, north). Cell = y*W + x.
// `cells` must hold at least b.c entries. Returns b.c.
inline int ringCells(const Box& b, int W, int H, int* cells) {
    int n = 0, x = b.x0, y = b.y0;
    for (int i = 0; i < b.w - 1; ++i) { cells[n++] = y * W + x; x = (x + 1) % W; }
    for (int i = 0; i < b.h - 1; ++i) { cells[n++] = y * W + x; y = (y + 1) % H; }
    for (int i = 0; i < b.w - 1; ++i) { cells[n++] = y * W + x; x = (x + W - 1) % W; }
    for (int i = 0; i < b.h - 1; ++i) { cells[n++] = y * W + x; y = (y + H - 1) % H; }
    return n;
}

// ---------------------------------------------------------------------------------------------
// Dash arithmetic (rules from the Execution Order page)
//   first ceil(L/4) steps of a move are free; every later step also removes one extra tail segment;
//   a dragon of length 2 cannot pay. After `steps` steps the trail is X, then (steps-1) interior tiles;
//   the body is the last endLen of them, so X is vacated iff endLen <= steps-1.
// ---------------------------------------------------------------------------------------------
inline bool dashParams(int Lq, int steps, int& endLen) {
    if (Lq < 2 || steps < 2 || steps > MAX_DASH) return false;
    int free_steps = (Lq + 3) / 4;
    int paid = steps > free_steps ? steps - free_steps : 0;
    if (paid > Lq - 2) return false;
    endLen = Lq - paid;
    return endLen <= steps - 1;
}

enum class Verdict {
    OK, BAD_PARAMS, RING_BAD, POCKET_OUTSIDE, LEAK, INTERIOR_UNSEEN, NOT_SEPARATING, NO_DASH
};

struct Plan {
    Box box;
    int Dx, Dy;                // waiting tile (outside the ring, next to the door tile X)
    int Xx, Xy;                // door tile (on the ring)
    int steps;                 // dash length
    int endLen;                // queen length after the dash
    int dirs[MAX_DASH];        // direction of each dash step, starting from D
    int qx, qy, qa, qb;        // queen cycle sub-box, box-local coordinates (qa wide, qb high)
    int yIndex, arcSign;       // index of the first interior tile on the queen cycle, travel sign
};

// Checks the ring really is a ring: all tiles seen, all travel edges PLAIN.
inline bool ringOK(const Snapshot& s, const Box& b) {
    int x = b.x0, y = b.y0;
    const int len[4] = {b.w - 1, b.h - 1, b.w - 1, b.h - 1};
    for (int d = 0; d < 4; ++d)
        for (int i = 0; i < len[d]; ++i) {
            if (!s.seen[y][x] || s.edge[y][x][d] != EDGE_PLAIN) return false;
            x = (x + DX[d] + s.W) % s.W;
            y = (y + DY[d] + s.H) % s.H;
        }
    return x == b.x0 && y == b.y0;
}

// Full validation of one box for a pocket tile (px,py), queen length Lq and dash length `steps`.
// Order: params, ring, pocket, sealing flood fill (LEAK > INTERIOR_UNSEEN), separation, door+dash.
inline Verdict validate(const Snapshot& s, const Box& b, int px, int py, int Lq, int steps, Plan& plan) {
    int endLen = 0;
    if (!dashParams(Lq, steps, endLen)) return Verdict::BAD_PARAMS;
    const int W = s.W, H = s.H;
    if (b.w < 4 || b.h < 4 || b.w > W - 1 || b.h > H - 1 || b.c > MAX_RING || b.c != 2 * (b.w + b.h) - 4)
        return Verdict::RING_BAD;
    if (!ringOK(s, b)) return Verdict::RING_BAD;

    auto lx = [&](int x) { return (x - b.x0 + W) % W; };
    auto ly = [&](int y) { return (y - b.y0 + H) % H; };
    // 0 outside, 1 ring, 2 interior
    auto cls = [&](int x, int y) {
        int l = lx(x), m = ly(y);
        if (l >= b.w || m >= b.h) return 0;
        if (l == 0 || l == b.w - 1 || m == 0 || m == b.h - 1) return 1;
        return 2;
    };
    if (cls(px, py) != 2) return Verdict::POCKET_OUTSIDE;

    // ---- sealing flood fill from the pocket. Ring tiles and kelp edges are walls. ----
    static bool inK[MAX_H][MAX_W];
    static uint16_t stack[MAX_W * MAX_H];
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) inK[y][x] = false;
    int sp = 0, ksize = 0;
    bool leak = false, unseenFlag = false;
    stack[sp++] = static_cast<uint16_t>(py * W + px);
    inK[py][px] = true;
    ++ksize;
    while (sp > 0) {
        int u = stack[--sp];
        int ux = u % W, uy = u / W;
        if (!s.seen[uy][ux]) unseenFlag = true;
        for (int d = 0; d < 4; ++d) {
            uint8_t k = s.edge[uy][ux][d];
            if (k == EDGE_KELP) continue;
            if (k == EDGE_PORTAL) { leak = true; continue; }
            if (k == EDGE_UNKNOWN) { unseenFlag = true; continue; }
            int vx = (ux + DX[d] + W) % W, vy = (uy + DY[d] + H) % H;
            int c = cls(vx, vy);
            if (c == 1) continue;                 // ring tile: wall
            if (c == 0) { leak = true; continue; }
            if (inK[vy][vx]) continue;
            inK[vy][vx] = true;
            ++ksize;
            stack[sp++] = static_cast<uint16_t>(vy * W + vx);
        }
    }
    if (leak) return Verdict::LEAK;
    if (unseenFlag) return Verdict::INTERIOR_UNSEEN;
    if (W * H - b.c - ksize < 1) return Verdict::NOT_SEPARATING;

    // ---- door + queen cycle + dash ----
    for (int side = 0; side < 4; ++side) {
        const int v = (side == 0) ? 1 : (side == 1) ? 2 : (side == 2) ? 3 : 0;   // inward direction
        const int count = (side % 2 == 0) ? b.w - 2 : b.h - 2;                   // non-corner tiles
        for (int t = 1; t <= count; ++t) {
            int xl, yl;                                                            // X in box-local coords
            if (side == 0)      { xl = t;       yl = 0; }
            else if (side == 1) { xl = b.w - 1; yl = t; }
            else if (side == 2) { xl = t;       yl = b.h - 1; }
            else                { xl = 0;       yl = t; }
            const int Xx = (b.x0 + xl) % W, Xy = (b.y0 + yl) % H;
            const int Dx = (Xx - DX[v] + W) % W, Dy = (Xy - DY[v] + H) % H;
            const int Yx = (Xx + DX[v] + W) % W, Yy = (Xy + DY[v] + H) % H;
            const int ylx = xl + DX[v], yly = yl + DY[v];                          // Y in box-local coords
            if (!s.seen[Xy][Xx] || s.risk[Xy][Xx]) continue;
            if (!s.seen[Dy][Dx]) continue;
            if (s.edge[Dy][Dx][v] != EDGE_PLAIN || s.edge[Xy][Xx][v] != EDGE_PLAIN) continue;
            if (!s.seen[Yy][Yx] || s.risk[Yy][Yx] || !inK[Yy][Yx]) continue;

            for (int qa = 2; qa <= 4; ++qa)
                for (int qb = 2; qb <= 4; ++qb) {
                    const int q = 2 * (qa + qb) - 4;
                    if (q < endLen + 1 || q < steps) continue;
                    for (int qx = 1; qx <= b.w - 1 - qa; ++qx)
                        for (int qy = 1; qy <= b.h - 1 - qb; ++qy) {
                            if (ylx < qx || ylx > qx + qa - 1 || yly < qy || yly > qy + qb - 1) continue;
                            if (!(ylx == qx || ylx == qx + qa - 1 || yly == qy || yly == qy + qb - 1)) continue;
                            // build the queen cycle (clockwise) in box-local coordinates
                            int qlx[16], qly[16], dd[16], n = 0, cx = qx, cy = qy, iy = -1;
                            const int lens[4] = {qa - 1, qb - 1, qa - 1, qb - 1};
                            for (int d = 0; d < 4; ++d)
                                for (int i = 0; i < lens[d]; ++i) {
                                    qlx[n] = cx; qly[n] = cy; dd[n] = d;
                                    if (cx == ylx && cy == yly) iy = n;
                                    ++n; cx += DX[d]; cy += DY[d];
                                }
                            if (iy < 0) continue;
                            bool okQ = true;
                            for (int i = 0; i < q && okQ; ++i) {
                                int j = (i + 1) % q;
                                int ax = (b.x0 + qlx[i]) % W, ay = (b.y0 + qly[i]) % H;
                                int bx = (b.x0 + qlx[j]) % W, by = (b.y0 + qly[j]) % H;
                                if (!s.seen[ay][ax] || !inK[ay][ax]) okQ = false;
                                else if (s.edge[ay][ax][dd[i]] != EDGE_PLAIN) okQ = false;
                                else if (s.edge[by][bx][(dd[i] + 2) & 3] != EDGE_PLAIN) okQ = false;
                            }
                            if (!okQ) continue;
                            for (int sign = 1; sign >= -1; sign -= 2) {
                                bool okArc = true;
                                int idx[MAX_DASH];
                                for (int k = 0; k <= steps - 2; ++k) {
                                    idx[k] = (((iy + sign * k) % q) + q) % q;
                                    int ax = (b.x0 + qlx[idx[k]]) % W, ay = (b.y0 + qly[idx[k]]) % H;
                                    if (s.risk[ay][ax]) { okArc = false; break; }
                                }
                                if (!okArc) continue;
                                plan.box = b;
                                plan.Dx = Dx; plan.Dy = Dy; plan.Xx = Xx; plan.Xy = Xy;
                                plan.steps = steps; plan.endLen = endLen;
                                plan.dirs[0] = v; plan.dirs[1] = v;
                                for (int k = 0; k + 1 <= steps - 2; ++k)
                                    plan.dirs[2 + k] = (sign == 1) ? dd[idx[k]] : ((dd[idx[k + 1]] + 2) & 3);
                                plan.qx = qx; plan.qy = qy; plan.qa = qa; plan.qb = qb;
                                plan.yIndex = iy; plan.arcSign = sign;
                                return Verdict::OK;
                            }
                        }
                }
        }
    }
    return Verdict::NO_DASH;
}

} // namespace bodyguard::rect
