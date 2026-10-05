#pragma once

#include "bodyguard/core/fixed_bitset.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace bodyguard_sim {

constexpr int MAX_WIDTH = 64;
constexpr int MAX_HEIGHT = 64;
constexpr int MAX_CELLS = MAX_WIDTH * MAX_HEIGHT;
constexpr int MAX_DRAGONS = 64;
constexpr int MAX_STEPS = 256;
constexpr std::uint8_t NORTH_EDGE = 1U << 0;
constexpr std::uint8_t EAST_EDGE = 1U << 1;
constexpr std::uint8_t SOUTH_EDGE = 1U << 2;
constexpr std::uint8_t WEST_EDGE = 1U << 3;

using Cell = int;

enum class Direction : std::uint8_t { North, East, South, West };
enum class DeathReason : std::uint8_t { None, Wall, Self, OtherBody, HeadToHead, NoValidAction };

struct Action {
    std::array<Direction, MAX_STEPS> steps{};
    std::size_t count = 0;

    bool push(Direction direction) {
        if (count == steps.size())
            return false;
        steps[count++] = direction;
        return true;
    }
};

struct Dragon {
    bool alive = false;
    int length = 0;
    std::array<Cell, MAX_CELLS> body{};
};

class World {
    int width_ = 0;
    int height_ = 0;
    std::array<std::uint8_t, MAX_CELLS> kelp_{};
    bot::bodyguard::core::FixedBitset<MAX_CELLS> pearls_;
    std::array<int, MAX_CELLS> occupant_{};
    std::array<Dragon, MAX_DRAGONS> dragons_{};
    int next_id_ = 0;

    static std::pair<int, int> offset(Direction direction) {
        switch (direction) {
        case Direction::North: return {0, -1};
        case Direction::East: return {1, 0};
        case Direction::South: return {0, 1};
        case Direction::West: return {-1, 0};
        }
        return {0, 0};
    }

    Cell cell(int x, int y) const {
        x = (x % width_ + width_) % width_;
        y = (y % height_ + height_) % height_;
        return y * width_ + x;
    }

    Cell next_cell(Cell source, Direction direction) const {
        int x = source % width_;
        int y = source / width_;
        auto [dx, dy] = offset(direction);
        return cell(x + dx, y + dy);
    }

    std::uint8_t edge_bit(Direction direction) const {
        switch (direction) {
        case Direction::North: return NORTH_EDGE;
        case Direction::East: return EAST_EDGE;
        case Direction::South: return SOUTH_EDGE;
        case Direction::West: return WEST_EDGE;
        }
        return 0;
    }

    void rebuild_occupancy() {
        occupant_.fill(-1);
        for (int dragon_id = 0; dragon_id < MAX_DRAGONS; ++dragon_id) {
            const Dragon& dragon = dragons_[dragon_id];
            if (!dragon.alive)
                continue;
            for (int index = 0; index < dragon.length; ++index)
                occupant_[dragon.body[index]] = dragon_id;
        }
    }

  public:
    void initialize(int width, int height) {
        width_ = width;
        height_ = height;
        kelp_.fill(0);
        pearls_.clear();
        occupant_.fill(-1);
        dragons_.fill({});
        next_id_ = 0;
    }

    int width() const { return width_; }
    int height() const { return height_; }
    Cell cell_at(int x, int y) const { return cell(x, y); }
    Cell head(int dragon_id) const { return dragons_[dragon_id].body[0]; }
    int length(int dragon_id) const { return dragons_[dragon_id].length; }
    bool alive(int dragon_id) const { return dragons_[dragon_id].alive; }
    int next_unused_id() const { return next_id_; }
    bool has_pearl(Cell position) const { return pearls_.test(static_cast<std::size_t>(position)); }
    DeathReason last_death = DeathReason::None;

    void add_dragon(int dragon_id, const std::array<Cell, MAX_CELLS>& body, int length) {
        dragons_[dragon_id].alive = true;
        dragons_[dragon_id].length = length;
        dragons_[dragon_id].body = body;
        if (dragon_id >= next_id_)
            next_id_ = dragon_id + 1;
        rebuild_occupancy();
    }

    void add_dragon(int dragon_id, Cell head_position) {
        std::array<Cell, MAX_CELLS> body{};
        body[0] = head_position;
        add_dragon(dragon_id, body, 1);
    }

    void set_kelp(Cell position, Direction direction, bool enabled = true) {
        if (enabled)
            kelp_[position] |= edge_bit(direction);
        else
            kelp_[position] &= static_cast<std::uint8_t>(~edge_bit(direction));
    }

    void set_pearl(Cell position, bool enabled = true) {
        if (enabled)
            pearls_.set(static_cast<std::size_t>(position));
        else
            pearls_.reset(static_cast<std::size_t>(position));
    }

    DeathReason move(int dragon_id, const Action& action) {
        last_death = DeathReason::None;
        Dragon& dragon = dragons_[dragon_id];
        if (!dragon.alive || action.count == 0) {
            last_death = DeathReason::NoValidAction;
            return last_death;
        }
        const int free_steps = (dragon.length + 3) / 4;
        for (std::size_t step = 0; step < action.count; ++step) {
            bool paid = static_cast<int>(step) >= free_steps;
            if (paid && dragon.length <= 2)
                return kill(dragon_id, DeathReason::NoValidAction);
            Cell destination = next_cell(dragon.body[0], action.steps[step]);
            if ((kelp_[dragon.body[0]] & edge_bit(action.steps[step])) != 0)
                return kill(dragon_id, DeathReason::Wall);
            int other_id = occupant_[destination];
            if (other_id >= 0) {
                if (other_id == dragon_id)
                    return kill(dragon_id, DeathReason::Self);
                bool other_head = dragons_[other_id].body[0] == destination;
                if (other_head) {
                    kill(other_id, DeathReason::HeadToHead);
                    return kill(dragon_id, DeathReason::HeadToHead);
                }
                return kill(dragon_id, DeathReason::OtherBody);
            }
            bool ate = pearls_.test(static_cast<std::size_t>(destination));
            int old_length = dragon.length;
            int new_length = old_length + (ate ? 1 : 0);
            for (int index = new_length - 1; index > 0; --index)
                dragon.body[index] = dragon.body[index - 1];
            dragon.body[0] = destination;
            if (ate)
                pearls_.reset(static_cast<std::size_t>(destination));
            if (paid)
                --new_length;
            dragon.length = new_length;
            rebuild_occupancy();
        }
        return last_death;
    }

    bool split(int parent_id, int child_size) {
        Dragon& parent = dragons_[parent_id];
        if (!parent.alive || child_size < 2 || parent.length - child_size < 2 || next_id_ >= MAX_DRAGONS) {
            kill(parent_id, DeathReason::NoValidAction);
            return false;
        }

        int child_id = next_id_++;
        Dragon& child = dragons_[child_id];
        child.alive = true;
        child.length = child_size;
        for (int index = 0; index < child_size; ++index)
            child.body[index] = parent.body[parent.length - 1 - index];
        parent.length -= child_size;
        rebuild_occupancy();
        return true;
    }

  private:
    DeathReason kill(int dragon_id, DeathReason reason) {
        const Dragon& dragon = dragons_[dragon_id];
        for (int index = 0; index < dragon.length; index += 2)
            pearls_.set(static_cast<std::size_t>(dragon.body[index]));
        dragons_[dragon_id].alive = false;
        last_death = reason;
        rebuild_occupancy();
        return reason;
    }
};

} // namespace bodyguard_sim
