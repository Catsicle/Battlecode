#include "helper.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#define LENGTH 1.0
#define KELP -1.0
#define OWN_BODY -0.5
#define ENEMY_HEAD -5.0
#define ENEMY_BODY -3.0 //Changed from -1
#define PORTAL 0.0
#define ALLY_HEAD -2.0 //Changed from -1
#define ALLY_BODY -1.5 // Changed from -1
#define NEG_SCORE -std::numeric_limits<double>::infinity()
#define POS_SCORE std::numeric_limits<double>::infinity()
#define DEPTH 12
#define UNEXPLORED_BONUS 10.0
#ifndef BOT_DEBUG
#define BOT_DEBUG 0
#endif

namespace {

using unswbc::Direction;
using unswbc::EdgeType;
using unswbc::Position;

constexpr int BOARD_SIDE = unswbc::Constants::VISION_SIZE;
constexpr int BOARD_CELLS = BOARD_SIDE * BOARD_SIDE;
constexpr int SEARCH_NODE_LIMIT = 5000;
using BoardMask = std::uint64_t;

struct SeenMap {
    int width;
    int height;
    std::vector<BoardMask> cells;

    SeenMap(int width, int height)
        : width(width), height(height), cells(static_cast<std::size_t>((width * height + 63) / 64), 0) {}

    int index(Position position) const {
        return position.y * width + position.x;
    }

    bool contains(Position position) const {
        if (position.x < 0 || position.x >= width || position.y < 0 || position.y >= height)
            return false;
        int const cell = index(position);
        return (cells[static_cast<std::size_t>(cell / 64)] & (1ULL << (cell % 64))) != 0;
    }

    void mark(Position position) {
        int const cell = index(position);
        cells[static_cast<std::size_t>(cell / 64)] |= 1ULL << (cell % 64);
    }
};

struct PortalEdge {
    int cell = -1;
    Direction direction = Direction::NORTH;
    int id = -1;
};

struct Board {
    std::array<Position, BOARD_CELLS> positions{};
    std::array<int, BOARD_CELLS> position_to_cell{};
    BoardMask live_pearls = 0;
    BoardMask dead_pearls = 0;
    BoardMask kelp = 0;
    BoardMask own_body = 0;
    BoardMask ally_body = 0;
    BoardMask ally_heads = 0;
    BoardMask enemy_body = 0;
    BoardMask enemy_heads = 0;
    BoardMask obstacles = 0;
    BoardMask portals = 0;
    std::array<BoardMask, 4> kelp_edges{};
    std::array<BoardMask, 4> portal_edges{};
    std::array<std::array<PortalEdge, 2>, 64> portal_pairs{};
    std::array<int, 64> portal_counts{};
    int head_cell = -1;
    Direction head_direction = Direction::NORTH;
};

struct SnakeState {
    std::array<int, BOARD_CELLS> body{};
    int body_size = 0;
    int length = 0;
    int head_cell = -1;
    Direction direction = Direction::NORTH;
    BoardMask body_mask = 0;
};

struct SearchResult {
    double score = NEG_SCORE;
    bool legal = false;
};

void update_seen_map(SeenMap& seen_map, unswbc::Controller const& controller) {
    for (auto const& tile : controller.get_tiles())
        seen_map.mark(tile.get_position());
}

int direction_index(Direction direction) {
    switch (direction.value) {
    case Direction::NORTH: return 0;
    case Direction::EAST: return 1;
    case Direction::SOUTH: return 2;
    default: return 3;
    }
}

BoardMask cell_bit(int cell) {
    return cell < 0 ? 0ULL : (1ULL << static_cast<unsigned>(cell));
}

int find_cell(Board const& board, Position position) {
    for (int cell = 0; cell < BOARD_CELLS; ++cell) {
        if (board.positions[cell] == position)
            return cell;
    }
    return -1;
}

BoardMask neighborhood(int cell) {
    if (cell < 0)
        return 0;
    int const x = cell % BOARD_SIDE;
    int const y = cell / BOARD_SIDE;
    BoardMask result = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int const nx = x + dx;
            int const ny = y + dy;
            if (nx >= 0 && nx < BOARD_SIDE && ny >= 0 && ny < BOARD_SIDE)
                result |= cell_bit(ny * BOARD_SIDE + nx);
        }
    }
    return result & ~cell_bit(cell);
}

Board build_board(unswbc::Controller const& controller) {
    Board board;
    auto const& tiles = controller.get_tiles();
    for (int cell = 0; cell < BOARD_CELLS; ++cell) {
        board.positions[cell] = tiles[static_cast<std::size_t>(cell)].get_position();
        board.position_to_cell[cell] = cell;
    }

    auto mark_dragon = [&](int cell, unswbc::DragonPart const& part) {
        BoardMask const bit = cell_bit(cell);
        if (part.get_id() == controller.get_id()) {
            board.own_body |= bit;
            if (part.is_head()) {
                board.head_cell = cell;
                board.head_direction = part.get_dir();
            }
        } else if (part.get_team() == controller.get_team()) {
            board.ally_body |= bit;
            if (part.is_head())
                board.ally_heads |= bit;
        } else {
            board.enemy_body |= bit;
            if (part.is_head())
                board.enemy_heads |= bit;
        }
    };

    for (int cell = 0; cell < BOARD_CELLS; ++cell) {
        auto const& tile = tiles[static_cast<std::size_t>(cell)];
        BoardMask const bit = cell_bit(cell);
        if (tile.has_pearl())
            board.live_pearls |= bit;
        else if (tile.get_pearl_time() >= 0)
            board.dead_pearls |= bit;
        if (auto const* part = tile.get_dragon())
            mark_dragon(cell, *part);

        for (Direction direction : Direction::get_direction_list()) {
            auto const& edge = tile.get_edge(direction);
            int const index = direction_index(direction);
            if (edge.get_edge_type() == static_cast<EdgeType>(1)) {
                board.kelp_edges[index] |= bit;
                board.kelp |= bit;
            } else if (edge.get_edge_type() == static_cast<EdgeType>(2)) {
                board.portals |= bit;
                board.portal_edges[index] |= bit;
                int const id = edge.get_portal_id();
                if (id >= 0 && id < static_cast<int>(board.portal_pairs.size()) && board.portal_counts[id] < 2) {
                    board.portal_pairs[id][board.portal_counts[id]++] = {cell, direction, id};
                }
            }
        }
    }
    board.obstacles = board.own_body | board.ally_body | board.enemy_body | board.kelp;
    return board;
}

SnakeState initial_snake(Board const& board, unswbc::Controller const& controller) {
    SnakeState state;
    state.length = controller.get_length();
    state.head_cell = board.head_cell;
    state.direction = board.head_direction;
    for (int cell = 0; cell < BOARD_CELLS; ++cell) {
        if ((board.own_body & cell_bit(cell)) != 0)
            state.body[state.body_size++] = cell;
    }

    // Reconstruct the visible head-to-tail chain by walking behind each segment.
    std::array<int, BOARD_CELLS> ordered{};
    BoardMask visited = 0;
    int count = 0;
    int current = board.head_cell;
    while (current >= 0 && count < state.body_size) {
        if ((visited & cell_bit(current)) != 0)
            break;
        ordered[count++] = current;
        visited |= cell_bit(current);
        auto const* current_tile = controller.get_tile(board.positions[current]);
        auto const* part = current_tile ? current_tile->get_dragon() : nullptr;
        if (!part)
            break;
        Direction const behind = part->get_dir().get_opposite();
        int const next = find_cell(board, board.positions[current].add_dir(behind));
        if (next < 0 || (board.own_body & cell_bit(next)) == 0)
            break;
        current = next;
    }
    for (int index = 0; index < count; ++index)
        state.body[index] = ordered[index];
    state.body_size = count;
    state.body_mask = board.own_body;
    return state;
}

int portal_destination(Board const& board, int cell, Direction direction) {
    int const id = [&]() {
        for (int candidate = 0; candidate < static_cast<int>(board.portal_pairs.size()); ++candidate) {
            if (board.portal_counts[candidate] != 2)
                continue;
            for (int endpoint = 0; endpoint < 2; ++endpoint) {
                if (board.portal_pairs[candidate][endpoint].cell == cell &&
                    board.portal_pairs[candidate][endpoint].direction == direction)
                    return candidate;
            }
        }
        return -1;
    }();
    if (id < 0)
        return -1;
    auto const& pair = board.portal_pairs[id];
    PortalEdge const& destination = pair[ pair[0].cell == cell && pair[0].direction == direction ? 1 : 0 ];
    Position const emerged = board.positions[destination.cell].add_dir(destination.direction.get_opposite());
    return find_cell(board, emerged);
}

int move_destination(Board const& board, int cell, Direction direction) {
    if (cell < 0)
        return -1;
    if ((board.kelp_edges[direction_index(direction)] & cell_bit(cell)) != 0)
        return -1;
    if ((board.portal_edges[direction_index(direction)] & cell_bit(cell)) != 0 &&
        portal_destination(board, cell, direction) < 0)
        return -1;
    int const portal_cell = portal_destination(board, cell, direction);
    if (portal_cell >= 0)
        return portal_cell;
    return find_cell(board, board.positions[cell].add_dir(direction));
}

bool simulate_move(Board const& board, SnakeState& state, Direction direction) {
    int const destination = move_destination(board, state.head_cell, direction);
    if (destination < 0)
        return false;
    BoardMask const destination_bit = cell_bit(destination);
    if ((state.body_mask & destination_bit) != 0 ||
        ((board.ally_body | board.enemy_body) & destination_bit) != 0)
        return false;

    bool const grows = (board.live_pearls & destination_bit) != 0;
    if (grows)
        ++state.length;
    if (!grows && state.body_size > 0) {
        state.body_mask &= ~cell_bit(state.body[state.body_size - 1]);
        --state.body_size;
    }
    if (state.body_size >= BOARD_CELLS)
        return false;
    for (int index = state.body_size; index > 0; --index)
        state.body[index] = state.body[index - 1];
    state.body[0] = destination;
    ++state.body_size;
    state.body_mask |= destination_bit;
    state.head_cell = destination;
    state.direction = direction;
    return true;
}

double reciprocal_score(BoardMask mask, int origin, Board const& board, double weight) {
    if (origin < 0)
        return 0.0;
    Position const source = board.positions[origin];
    double score = 0.0;
    while (mask != 0) {
        int const cell = static_cast<int>(__builtin_ctzll(mask));
        mask &= mask - 1;
        Position const target = board.positions[cell];
        int const distance = std::abs(source.x - target.x) + std::abs(source.y - target.y);
        if (distance > 0)
            score += weight / static_cast<double>(distance);
    }
    return score;
}

double evaluate(Board const& board, SnakeState const& state) {
    double score = 0.0;
    score += static_cast<double>(state.length) * LENGTH;
    score += reciprocal_score(board.kelp, state.head_cell, board, KELP);
    score += reciprocal_score(board.enemy_heads, state.head_cell, board, ENEMY_HEAD);
    score += reciprocal_score(board.enemy_body & ~board.enemy_heads, state.head_cell, board, ENEMY_BODY);
    score += reciprocal_score(board.portals, state.head_cell, board, PORTAL);
    score += reciprocal_score(board.ally_heads, state.head_cell, board, ALLY_HEAD);
    score += reciprocal_score(board.ally_body & ~board.ally_heads, state.head_cell, board, ALLY_BODY);
    BoardMask const nearby = neighborhood(state.head_cell);
    score += static_cast<double>(__builtin_popcountll(nearby & state.body_mask)) / 8.0 * OWN_BODY;
    return score;
}

bool unexplored_move(unswbc::Controller const& controller, SeenMap const& seen_map, Direction direction) {
    auto const* here = controller.get_tile(controller.get_position());
    if (!here || !here->get_edge(direction).is_passable() || here->get_edge(direction).is_portal())
        return false;
    Position const destination = controller.get_position().add_dir(direction);
    if (controller.get_tile(destination) != nullptr)
        return false;
    return !seen_map.contains(destination);
}

SearchResult search(Board const& board, SnakeState const& state, int depth, int& nodes) {
    if (depth == 0 || nodes >= SEARCH_NODE_LIMIT)
        return {evaluate(board, state), true};
    ++nodes;

    SearchResult best;
    for (Direction direction : Direction::get_direction_list()) {
        SnakeState next = state;
        if (!simulate_move(board, next, direction))
            continue;
        SearchResult result = search(board, next, depth - 1, nodes);
        if (!best.legal || result.score > best.score) {
            best = {result.score, true};
        }
    }
    return best.legal ? best : SearchResult{NEG_SCORE, false};
}

#if BOT_DEBUG
bool raw_engine_move_is_available(unswbc::Controller const& controller, Direction direction) {
    auto const* here = controller.get_tile(controller.get_position());
    if (!here || !here->get_edge(direction).is_passable())
        return false;
    auto const* ahead = controller.get_tile(controller.get_position().add_dir(direction));
    return ahead == nullptr || ahead->get_dragon() == nullptr;
}

void debug_body_state(unswbc::Controller& controller, Board const& board, SnakeState const& state) {
    controller.output_log("DBG_BODY", "round", unswbc::game->get_round_num(), "head", board.head_cell,
                          "visible", state.body_size, "mask", state.body_mask);
    for (int index = 0; index < state.body_size; ++index) {
        int const cell = state.body[index];
        if (cell < 0 || cell >= BOARD_CELLS) {
            controller.output_log("DBG_BODY_INVALID", "index", index, "cell", cell);
            continue;
        }
        auto const position = board.positions[cell];
        controller.output_log("DBG_BODY_PART", "index", index, "cell", cell, "x", position.x, "y", position.y);
    }
}

void debug_root_candidates(unswbc::Controller& controller, Board const& board, SnakeState const& state) {
    for (Direction direction : Direction::get_direction_list()) {
        SnakeState next = state;
        bool const simulated = simulate_move(board, next, direction);
        bool const raw_available = raw_engine_move_is_available(controller, direction);
        int destination = simulated ? next.head_cell : -1;
        double score = NEG_SCORE;
        int nodes = 0;
        if (simulated)
            score = search(board, next, DEPTH - 1, nodes).score;

        auto const* here = controller.get_tile(controller.get_position());
        int edge_type = -1;
        int portal_id = -1;
        if (here) {
            edge_type = static_cast<int>(here->get_edge(direction).get_edge_type());
            portal_id = here->get_edge(direction).get_portal_id();
        }
        controller.output_log("DBG_MOVE", "dir", direction, "raw", raw_available, "sim", simulated,
                              "edge", edge_type, "portal", portal_id, "dest", destination,
                              "score", score, "nodes", nodes);
    }
}
#endif

Direction choose_move(Board const& board, SnakeState const& state, unswbc::Controller& controller,
                      SeenMap const& seen_map) {
#if !BOT_DEBUG
    (void)controller;
#endif
    Direction chosen = Direction::NORTH;
    double best_score = NEG_SCORE;
    bool found_legal_move = false;
    int nodes = 0;
    for (Direction direction : Direction::get_direction_list()) {
        SnakeState next = state;
        bool const simulated = simulate_move(board, next, direction);
        if (!simulated && !unexplored_move(controller, seen_map, direction))
            continue;
        double score = simulated ? search(board, next, DEPTH - 1, nodes).score : evaluate(board, state);
        if (!seen_map.contains(controller.get_position().add_dir(direction)))
            score += UNEXPLORED_BONUS;
        if (!found_legal_move || score > best_score) {
            best_score = score;
            chosen = direction;
            found_legal_move = true;
        }
    }
#if BOT_DEBUG
    controller.output_log("DBG_CHOSEN", "dir", chosen, "raw", raw_engine_move_is_available(controller, chosen),
                          "nodes", nodes);
#endif
    return chosen;
}

} // namespace

int main() {
    auto [ct, game] = unswbc::init();
    SeenMap seen_map(game.width, game.height);
    while (unswbc::update(ct, game)) {
        update_seen_map(seen_map, ct);
        Board const board = build_board(ct);
        SnakeState const state = initial_snake(board, ct);
    #if BOT_DEBUG
        debug_body_state(ct, board, state);
        debug_root_candidates(ct, board, state);
    #endif
        bool has_legal_move = false;
        for (Direction direction : Direction::get_direction_list()) {
            SnakeState next = state;
            if (simulate_move(board, next, direction) || unexplored_move(ct, seen_map, direction)) {
                has_legal_move = true;
                break;
            }
        }

        Direction chosen = Direction::NORTH;
        if (has_legal_move)
            chosen = choose_move(board, state, ct, seen_map);

        int const tail_split_size = ct.get_length() - 2;
        bool const chosen_enters_portal = has_legal_move &&
            (board.portal_edges[direction_index(chosen)] & cell_bit(state.head_cell)) != 0;
        if ((!has_legal_move || chosen_enters_portal) && ct.can_split(tail_split_size))
            ct.do_split(tail_split_size);
        else
            ct.make_move(chosen);
        unswbc::end_turn();
    }
}