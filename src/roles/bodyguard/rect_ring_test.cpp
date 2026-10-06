// g++ -std=c++20 -O2 -Wall -Wextra -pedantic rect_ring_test.cpp -o rect_ring_test && ./rect_ring_test
#include "rect_ring.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <set>
#include <tuple>
#include <vector>
using namespace bodyguard::rect;

static Snapshot S;   // ~100 KB: keep static
static std::mt19937 rng(12345);
static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; if (failures <= 15) { printf("FAIL: " __VA_ARGS__); printf("\n"); } } } while (0)

// consistent random map: edge kinds are symmetric, unseen tiles random
static void randomMap(int W, int H, int pKelp, int pPortal, int pUnknown, int pUnseen) {
    S.reset(W, H);
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) { S.seen[y][x] = (int)(rng() % 100) >= pUnseen; S.risk[y][x] = rng() % 100 < 3; }
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) for (int d = 0; d < 2; ++d) {
        int r = rng() % 100; uint8_t k = EDGE_PLAIN;
        if (r < pKelp) k = EDGE_KELP; else if (r < pKelp + pPortal) k = EDGE_PORTAL; else if (r < pKelp + pPortal + pUnknown) k = EDGE_UNKNOWN;
        int nx = (x + DX[d] + W) % W, ny = (y + DY[d] + H) % H;
        S.edge[y][x][d] = k; S.edge[ny][nx][(d + 2) & 3] = k;
    }
    S.finalize();
}
static void openMap(int W, int H) { randomMap(W, H, 0, 0, 0, 0); for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) S.risk[y][x] = false; }

// ---------------- oracle 1: explicit ring list ----------------
static bool oracleRing(int x0, int y0, int w, int h) {
    const int W = S.W, H = S.H; std::vector<std::tuple<int,int,int>> t; int x = x0, y = y0;
    for (int i = 0; i < w - 1; ++i) { t.push_back({x,y,0}); x = (x + 1) % W; }
    for (int i = 0; i < h - 1; ++i) { t.push_back({x,y,1}); y = (y + 1) % H; }
    for (int i = 0; i < w - 1; ++i) { t.push_back({x,y,2}); x = (x + W - 1) % W; }
    for (int i = 0; i < h - 1; ++i) { t.push_back({x,y,3}); y = (y + H - 1) % H; }
    std::set<std::pair<int,int>> uniq;
    for (auto& [a, b, d] : t) {
        if (!S.seen[b][a]) return false;
        if (S.edge[b][a][d] != EDGE_PLAIN) return false;
        uniq.insert({a, b});
    }
    return uniq.size() == t.size();
}
static bool oracleUnseen(int x0, int y0, int w, int h) {
    for (int j = 1; j <= h - 2; ++j) for (int i = 1; i <= w - 2; ++i) if (!S.seen[(y0 + j) % S.H][(x0 + i) % S.W]) return true;
    return false;
}

static void testGenerator() {
    long boxes = 0, maps = 0;
    for (int it = 0; it < 4000; ++it) {
        int W = 6 + rng() % 11, H = 6 + rng() % 11, cap = 12 + 2 * (rng() % (W + H));
        int pk = rng() % 12, pu = rng() % 10;
        randomMap(W, H, pk, rng() % 3, rng() % 3, pu);
        int px = -1, py = -1; if (rng() % 2) { px = rng() % W; py = rng() % H; }
        std::set<std::tuple<int,int,int,int>> got; std::vector<Box> v;
        forEachBox(S, px, py, cap, [&](const Box& b) { got.insert({b.x0, b.y0, b.w, b.h}); v.push_back(b); });
        CHECK(got.size() == v.size(), "duplicate boxes emitted");
        std::set<std::tuple<int,int,int,int>> want;
        for (int h = 4; h <= H - 1; ++h) for (int w = 4; w <= W - 1; ++w) {
            if (2 * (w + h) - 4 > std::min(cap, MAX_RING)) continue;
            for (int y0 = 0; y0 < H; ++y0) for (int x0 = 0; x0 < W; ++x0) {
                if (px >= 0) { int dx = (px - x0 + W) % W, dy = (py - y0 + H) % H; if (dx < 1 || dx > w - 2 || dy < 1 || dy > h - 2) continue; }
                if (oracleRing(x0, y0, w, h)) want.insert({x0, y0, w, h});
            }
        }
        CHECK(got == want, "box set differs (W=%d H=%d cap=%d pocket=%d,%d got=%zu want=%zu)", W, H, cap, px, py, got.size(), want.size());
        for (auto& b : v) CHECK(b.unseen == oracleUnseen(b.x0, b.y0, b.w, b.h), "unseen flag wrong");
        boxes += (long)got.size(); ++maps;
        // top-N must equal the N best of the full set
        Box top[8]; int n = generate(S, px, py, cap, top, 8);
        std::vector<Box> sorted = v; std::sort(sorted.begin(), sorted.end(), [](const Box& a, const Box& b) { return better(a, b); });
        CHECK(n == (int)std::min<size_t>(8, sorted.size()), "top-N count");
        for (int i = 0; i < n && i < (int)sorted.size(); ++i) CHECK(top[i].x0 == sorted[i].x0 && top[i].y0 == sorted[i].y0 && top[i].w == sorted[i].w && top[i].h == sorted[i].h, "top-N order");
        // ringCells matches oracle ordering/length
        for (int i = 0; i < n; ++i) { int cells[MAX_RING]; int c = ringCells(top[i], W, H, cells); CHECK(c == top[i].c, "ringCells length");
            std::set<int> u(cells, cells + c); CHECK((int)u.size() == c, "ringCells not distinct"); }
    }
    printf("generator: %ld maps, %ld boxes compared with brute force\n", maps, boxes);
    openMap(32, 32);
    long n = 0; forEachBox(S, -1, -1, 40, [&](const Box&) { ++n; });
    CHECK(n == 122880, "open 32x32 cap40 count %ld", n);
    n = 0; forEachBox(S, 10, 10, 40, [&](const Box&) { ++n; });
    CHECK(n == 4540, "open 32x32 cap40 pocket count %ld", n);
    printf("open-map counts: 122880 / 4540 checked\n");
}

// ---------------- dash physics oracle (rules from Execution Order) ----------------
struct SimResult { bool alive; int len; };
static SimResult simStraight(int Lq, int steps, bool& xClear) {
    // open world, straight line; tile i along the line: head at 0, door X at 1
    std::vector<int> body; for (int i = 0; i < Lq; ++i) body.push_back(-i);   // head first
    int f = (Lq + 3) / 4;
    for (int st = 0; st < steps; ++st) {
        bool paid = st >= f;
        if (paid && (int)body.size() <= 2) { xClear = false; return {false, 0}; }
        int nh = body.front() + 1;
        for (int t : body) if (t == nh) return {false, 0};
        body.insert(body.begin(), nh); body.pop_back(); if (paid) body.pop_back();
    }
    xClear = true; for (int t : body) if (t == 1) xClear = false;
    return {true, (int)body.size()};
}
static void testDashParams() {
    int agree = 0;
    for (int Lq = 2; Lq <= 7; ++Lq) for (int st = 2; st <= MAX_DASH; ++st) {
        bool xc = false; SimResult r = simStraight(Lq, st, xc); int endLen = 0;
        bool p = dashParams(Lq, st, endLen);
        bool simOk = r.alive && xc;
        CHECK(p == simOk, "dashParams(%d,%d)=%d but sim says %d", Lq, st, p, simOk);
        if (p && simOk) CHECK(endLen == r.len, "endLen mismatch Lq=%d s=%d %d vs %d", Lq, st, endLen, r.len);
        ++agree;
    }
    printf("dashParams: %d (Lq,steps) pairs agree with movement simulation\n", agree);
}

// ---------------- oracle 2: sealing by fixed-point relaxation ----------------
static Verdict oracleSeal(const Box& b, int px, int py) {
    const int W = S.W, H = S.H;
    if (!oracleRing(b.x0, b.y0, b.w, b.h)) return Verdict::RING_BAD;
    auto inter = [&](int x, int y) { int l = (x - b.x0 + W) % W, m = (y - b.y0 + H) % H; return l >= 1 && l <= b.w - 2 && m >= 1 && m <= b.h - 2; };
    if (!inter(px, py)) return Verdict::POCKET_OUTSIDE;
    std::vector<char> in(W * H, 0); in[py * W + px] = 1; bool ch = true;
    while (ch) { ch = false;
        for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) if (in[y * W + x])
            for (int d = 0; d < 4; ++d) { if (S.edge[y][x][d] != EDGE_PLAIN) continue; int nx = (x + DX[d] + W) % W, ny = (y + DY[d] + H) % H;
                if (inter(nx, ny) && !in[ny * W + nx]) { in[ny * W + nx] = 1; ch = true; } } }
    bool leak = false, uns = false;
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) if (in[y * W + x]) {
        if (!S.seen[y][x]) uns = true;
        for (int d = 0; d < 4; ++d) { uint8_t k = S.edge[y][x][d]; if (k == EDGE_PORTAL) leak = true; if (k == EDGE_UNKNOWN) uns = true; } }
    if (leak) return Verdict::LEAK;
    if (uns) return Verdict::INTERIOR_UNSEEN;
    return Verdict::OK;   // sealed (dash may still fail)
}

// ---------------- oracle 3: simulate the dash on the real plan ----------------
static bool simPlan(const Plan& p, int Lq) {
    const int W = S.W, H = S.H; const Box& b = p.box;
    int ringc[MAX_RING]; int c = ringCells(b, W, H, ringc);
    std::set<int> wall; for (int i = 0; i < c; ++i) wall.insert(ringc[i]);
    int X = p.Xy * W + p.Xx; wall.erase(X);                         // the free tile
    // queen body trailing straight behind D, opposite the dash direction
    std::vector<int> body; int v = p.dirs[0]; int x = p.Dx, y = p.Dy;
    for (int i = 0; i < Lq; ++i) { body.push_back(y * W + x); x = (x - DX[v] + W) % W; y = (y - DY[v] + H) % H; }
    for (int t : body) { if (wall.count(t)) return true; /* unrealistic placement: skip */ }
    int f = (Lq + 3) / 4;
    for (int st = 0; st < p.steps; ++st) {
        bool paid = st >= f; if (paid && (int)body.size() <= 2) return false;
        int hx = body.front() % W, hy = body.front() / W; int d = p.dirs[st];
        int nh = ((hy + DY[d] + H) % H) * W + (hx + DX[d] + W) % W;
        for (int t : body) if (t == nh) return false;
        if (wall.count(nh)) return false;
        body.insert(body.begin(), nh); body.pop_back(); if (paid) body.pop_back();
    }
    if ((int)body.size() != p.endLen) return false;
    for (int t : body) if (t == X) return false;                       // door tile vacated
    // body must lie on the queen cycle; rebuild it
    std::vector<int> Q; int cx = p.qx, cy = p.qy; const int lens[4] = {p.qa - 1, p.qb - 1, p.qa - 1, p.qb - 1};
    for (int d = 0; d < 4; ++d) for (int i = 0; i < lens[d]; ++i) { Q.push_back(((b.y0 + cy) % H) * W + (b.x0 + cx) % W); cx += DX[d]; cy += DY[d]; }
    std::set<int> qs(Q.begin(), Q.end()); for (int t : body) if (!qs.count(t)) return false;
    int q = (int)Q.size(); int next = Q[(((p.yIndex + p.arcSign * (p.steps - 1)) % q) + q) % q];
    for (int t : body) if (t == next) return false;                     // head's next tile is free
    return true;
}

static void testValidator() {
    long cases = 0, ok = 0, sealCmp = 0, sims = 0; long byVerdict[8] = {0};
    for (int it = 0; it < 3000; ++it) {
        int W = 16 + rng() % 12, H = 16 + rng() % 12;
        randomMap(W, H, rng() % 4, rng() % 2, rng() % 2, rng() % 3);
        int px = 4 + rng() % (W - 8), py = 4 + rng() % (H - 8);
        Box top[12]; int n = generate(S, px, py, 24, top, 12);
        for (int i = 0; i < n; ++i) {
            int Lq = 4 + rng() % 4; int st = 2 + rng() % 6; int endLen;
            if (!dashParams(Lq, st, endLen)) continue;
            Plan p; Verdict vd = validate(S, top[i], px, py, Lq, st, p); ++cases; ++byVerdict[(int)vd];
            Verdict o = oracleSeal(top[i], px, py);
            if (vd == Verdict::NOT_SEPARATING) { CHECK(false, "NOT_SEPARATING should be impossible for w,h<=W-1"); continue; }
            if (vd == Verdict::OK || vd == Verdict::NO_DASH) CHECK(o == Verdict::OK, "validator passed sealing but oracle says %d", (int)o);
            else CHECK(o == vd, "seal verdict %d vs oracle %d", (int)vd, (int)o);
            ++sealCmp;
            if (vd == Verdict::OK) { ++ok; ++sims; CHECK(simPlan(p, Lq), "dash simulation failed (Lq=%d steps=%d)", Lq, st); }
        }
    }
    printf("validator: %ld cases, sealing verdict compared with fixed-point oracle: %ld, OK plans: %ld, dash-simulated: %ld\n", cases, sealCmp, ok, sims);
    printf("  verdict histogram: OK=%ld BAD_PARAMS=%ld RING_BAD=%ld POCKET_OUTSIDE=%ld LEAK=%ld INTERIOR_UNSEEN=%ld NOT_SEP=%ld NO_DASH=%ld\n",
           byVerdict[0], byVerdict[1], byVerdict[2], byVerdict[3], byVerdict[4], byVerdict[5], byVerdict[6], byVerdict[7]);
}

static void testKnownPlan() {
    openMap(32, 32);
    Box top[1]; int n = generate(S, 16, 16, 14, top, 1);
    CHECK(n == 1 && top[0].c == 14 && top[0].w * top[0].h == 20, "smallest-ring pick");
    // a 4x5 box around the pocket: ring c=14, interior 2x3
    Plan p; Verdict v = validate(S, top[0], 16, 16, 7, 5, p);
    CHECK(v == Verdict::OK, "known 4x5 open plan not OK (%d)", (int)v);
    if (v == Verdict::OK) { CHECK(p.endLen == 4, "endLen"); CHECK(simPlan(p, 7), "known plan dash sim"); }
    printf("known case: c=14 ring, Lq=7, 5-step dash -> %s, queen ends at length %d\n", v == Verdict::OK ? "OK" : "FAIL", v == Verdict::OK ? p.endLen : -1);
}

int main() {
    testGenerator();
    testDashParams();
    testValidator();
    testKnownPlan();
    printf(failures ? "\n%d FAILURES\n" : "\nALL TESTS PASSED\n", failures);
    return failures ? 1 : 0;
}
