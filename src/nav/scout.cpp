// #include "grid.hpp"
// #include "transition.hpp"
// #include "../config.hpp"

// #include <algorithm>
// #include <array>
// #include <cstdlib>
// #include <limits>
// #include <queue>
// #include <vector>

// namespace bot::nav {

// struct QueueNode {
//     int cost;
//     int cell;
//     bool operator>(const QueueNode& other) const { return cost > other.cost; }
// };

// static Path scout_search(const Grid& grid, Vec2 start_position, Vec2 target_position,
//                          int forbidden_first_direction) {
//     Path result;
//     int start = grid.memory.index(start_position);
//     int target = grid.memory.index(target_position);
//     if (start < 0 || target < 0 || !grid.known(start))
//         return result;
        
//     const int n = grid.memory.cells;
//     std::vector<int> distance(n, std::numeric_limits<int>::max());
//     std::vector<int> parent(n, -1);
//     std::vector<int> parent_dir(n, -1);
//     std::priority_queue<QueueNode, std::vector<QueueNode>, std::greater<QueueNode>> queue;
    
//     distance[start] = 0;
//     queue.push({0, start});

//     while (!queue.empty()) {
//         QueueNode current = queue.top();
//         queue.pop();
//         if (current.cost != distance[current.cell])
//             continue;
//         if (current.cell == target)
//             break;
            
//         Vec2 p = grid.memory.position(current.cell);
        
//         for (Dir direction : directions()) {
//             int d = dir_index(direction);
//             if (current.cell == start && ((forbidden_first_direction & (1U << d)) != 0))
//                 continue;
//             if (!grid.passable_edge(current.cell, d))
//                 continue;
                
//             Vec2 next_position = add(p, direction, grid.memory.width, grid.memory.height);
//             int next = grid.memory.index(next_position);
            
//             if (!grid.known(next))
//                 continue;
//             if (grid.memory.occ_fresh(next, grid.round))
//                 continue;

//             int step_cost = C_STEP;

//             // SCORCHED EARTH PENALTY
//             if (grid.memory.last_visit_round[next] >= 0) {
//                 step_cost += 500; 
//             }

//             int candidate = current.cost + step_cost;
//             if (candidate >= distance[next])
//                 continue;
                
//             distance[next] = candidate;
//             parent[next] = current.cell;
//             parent_dir[next] = d;
//             queue.push({candidate, next});
//         }
//     }
    
//     if (distance[target] == std::numeric_limits<int>::max())
//         return result;
        
//     std::vector<Dir> reversed;
//     for (int cell = target; cell != start && cell >= 0 && reversed.size() < 256; cell = parent[cell]) {
//         reversed.push_back(directions()[parent_dir[cell]]);
//     }
    
//     int count = static_cast<int>(reversed.size());
//     if (count == 0 && start != target)
//         return result;
        
//     for (int i = 0; i < count; ++i)
//         result.steps.push_back(reversed[count - i - 1]);
        
//     result.length = count;
//     result.cost = distance[target];
//     result.found = true;
//     return result;
// }

// Path scout_wander(const Grid& grid, Vec2 start, int length, std::uint8_t forbid_mask) {
//     (void)length;

//     // 1. IMMEDIATE PORTAL ENTRY
//     // If standing on a cell with an UNRESOLVED portal, force a step into it immediately
//     int current_cell = grid.memory.index(start);
//     if (current_cell >= 0) {
//         for (Dir direction : directions()) {
//             int d = dir_index(direction);
//             if (grid.memory.portal_id[current_cell][d] >= 0) {
//                 Step step = resolve_step(grid.memory, current_cell, d);
//                 if (step.kind == StepKind::PortalUnresolved) {
//                     if ((forbid_mask & (1U << d)) == 0) {
//                         Path path;
//                         path.steps.push_back(direction);
//                         path.length = 1;
//                         path.cost = C_STEP;
//                         path.found = true;
//                         return path;
//                     }
//                 }
//             }
//         }
//     }

//     struct Candidate {
//         int cell;
//         int score;
//     };
//     std::vector<Candidate> candidates;
//     candidates.reserve(grid.memory.cells);

//     for (int cell = 0; cell < grid.memory.cells; ++cell) {
//         if (!grid.known(cell))
//             continue;

//         bool has_unresolved_portal = false;
//         bool has_resolved_portal = false;

//         for (int d = 0; d < 4; ++d) {
//             if (grid.memory.portal_id[cell][d] >= 0) {
//                 Step step = resolve_step(grid.memory, cell, d);
//                 if (step.kind == StepKind::PortalUnresolved) {
//                     has_unresolved_portal = true;
//                 } else if (step.kind == StepKind::PortalOK) {
//                     has_resolved_portal = true;
//                 }
//             }
//         }

//         // PENALIZE VISITED CELLS -- UNLESS it contains an UNRESOLVED portal!
//         if (grid.memory.last_visit_round[cell] >= 0 && !has_unresolved_portal) {
//             continue;
//         }

//         int base_score = 5000;

//         if (has_unresolved_portal) {
//             base_score += 10000; // Prioritize unresolved portals
//         } else if (has_resolved_portal) {
//             base_score += 0;
//         }

//         Vec2 target_pos = grid.memory.position(cell);
//         int dx = std::abs(target_pos.x - start.x);
//         int dy = std::abs(target_pos.y - start.y);
//         dx = std::min(dx, grid.memory.width - dx);
//         dy = std::min(dy, grid.memory.height - dy);
//         int distance = dx + dy;

//         int final_score = base_score - distance;
//         candidates.push_back({cell, final_score});
//     }

//     std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
//         return a.score > b.score;
//     });

//     for (const auto& cand : candidates) {
//         Path path = scout_search(grid, start, grid.memory.position(cand.cell), forbid_mask);
//         if (path.found && path.length > 0)
//             return path;
//     }

//     return {};
// }

// } // namespace bot::nav