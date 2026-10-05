# Planned queries module (`src/core/memory/queries`)
# Clarified choices (user confirmation implied by instruction to proceed):
# - Path: src/core/memory/queries
# - Direction mapping: 0=N, 1=E, 2=S, 3=W (query spec, not bot::Dir)
# - Data source: Memory adapter (usable/plain/seen/occupancy/pearl)
# - Return: std::optional<int> for get_edge; std::pair<std::optional<int>, std::optional<int>> for get_cell
# - Not yet wired to M2/M3
# - Implementation: full return-case handling for all cases (NULL, unknown, empty, kelp, portal, etc.)
