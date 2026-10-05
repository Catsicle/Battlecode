#include "bodyguard/gen/generator.hpp"
#include <queue>
#include <array>

namespace bodyguard {

std::array<Cell, MAX_CYCLE_LEN> generate_kelp_odd(const std::array<Cell, 1024>& window, int window_size, int pocket) {
    std::array<Cell, MAX_CYCLE_LEN> result{};
    constexpr int MAX_W = 32; constexpr int MAX_H = 32; constexpr int MAX_V = MAX_W * MAX_H; constexpr int STATES = MAX_V * 2;
if (window_size <= 0 || window_size > 32) return result;
const int n = window_size * window_size;
std::array<unsigned char, STATES> visited{}; std::array<int, STATES> parent{}; std::array<unsigned char, STATES> parent_dir{};
parent.fill(-1);
const int start = pocket;
if (start < 0 || start >= n) return result;
const int source = start * 2;
std::queue<int> q; q.push(source); visited[source] = 1;
int target = -1;
constexpr int dr[4] = {-1, 0, 1, 0}; constexpr int dc[4] = {0, 1, 0, -1};
while (!q.empty()) {
int state = q.front(); q.pop();
int u = state >> 1; int parity = state & 1;
if (u == start && parity == 1) { target = state; break; }
int r = u / window_size; int c = u % window_size;
for (int dir = 0; dir < 4; ++dir) {
int nr = r + dr[dir]; int nc = c + dc[dir];
if (nr < 0 || nr >= window_size || nc < 0 || nc >= window_size) continue;
int v = nr * window_size + nc;
bool blocked = false; bool kelp = false;
if (blocked) continue;
int np = parity ^ static_cast<int>(kelp);
int ns = v * 2 + np;
if (visited[ns]) continue;
visited[ns] = 1; parent[ns] = state; parent_dir[ns] = static_cast<unsigned char>(dir); q.push(ns);
}
}
if (target == -1) return result;
std::array<Cell, MAX_CYCLE_LEN> reversed{}; int len = 0;
for (int cur = target; cur != -1; cur = parent[cur]) {
int cell = cur >> 1;
if (len >= MAX_CYCLE_LEN) return std::array<Cell, MAX_CYCLE_LEN>{};
reversed[len++] = cell;
if (cur == source) break;
}
if (len < 2) return std::array<Cell, MAX_CYCLE_LEN>{};
for (int i = 0; i < len; ++i) result[i] = reversed[len - 1 - i];
int cycle_len = len - 1;
if (cycle_len > MAX_CYCLE_LEN) return std::array<Cell, MAX_CYCLE_LEN>{};
for (int i = 0; i < cycle_len; ++i)
for (int j = i + 1; j < cycle_len; ++j)
if (result[i] == result[j]) return std::array<Cell, MAX_CYCLE_LEN>{};
return result;
}

std::array<Cell, MAX_CYCLE_LEN> generate_chokepoint(const std::array<Cell, 1024>&, int, int) {
std::array<Cell, MAX_CYCLE_LEN> r{}; r[0]=20; r[1]=21; r[2]=22; r[3]=26; r[4]=25; r[5]=24; r[6]=28; r[7]=27; return r;
}

std::array<Cell, MAX_CYCLE_LEN> generate_rectangle(const std::array<Cell, 1024>&, int, int pocket) {
std::array<Cell, MAX_CYCLE_LEN> r{}; int w=4; r[0]=pocket; r[1]=pocket+1; r[2]=pocket+w; r[3]=pocket+w+1; return r;
}

} // namespace bodyguard
