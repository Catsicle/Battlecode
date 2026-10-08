#pragma once
#include <optional>
#include <utility>
#include <limits>
#include "../memory.hpp"

namespace bot {

std::pair<std::optional<int>, std::optional<int>> get_cell(const Memory& memory, int x, int y);
std::pair<std::optional<int>, std::optional<int>> get_edge(const Memory& memory, int x, int y, int dir);

int mapview_width(const Memory& memory);
int mapview_height(const Memory& memory);
int mapview_cells(const Memory& memory);
bool mapview_valid(const Memory& memory, int cell);

bool query_known(const Memory& memory, int cell);
bool query_frontier(const Memory& memory, int cell);
int query_last_seen_round(const Memory& memory, int cell);
bool query_has_pearl(const Memory& memory, int cell);
int query_pearl_spawn_round(const Memory& memory, int cell);
bool query_never_spawns(const Memory& memory, int cell);
bool query_seen(const Memory& memory, int cell);
bool query_edge_known(const Memory& memory, int cell, int direction);

} // namespace bot
