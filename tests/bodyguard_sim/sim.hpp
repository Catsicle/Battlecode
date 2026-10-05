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
enum class SonarHit : std::uint8_t { Kelp, Ally, AllyHead, Enemy, EnemyHead };

struct Action {
    std::array<Direction, MAX_STEPS> steps{};
    std::size_t count = 0;
    std::array<std::uint64_t, 4> sonar_messages{};
    std::array<bool, 4> sonar_set{};

    bool push(Direction direction) {
        if (count == steps.size())
            return false;
        steps[count++] = direction;
        return true;
    }

    void send_sonar(Direction direction, std::uint64_t message) {
        int index = static_cast<int>(direction);
        sonar_messages[index] = message;
        sonar_set[index] = true;
    }
};

enum class ActionKind : std::uint8_t { None, Move, Split };

struct TurnAction {
    ActionKind kind = ActionKind::None;
    Action movement{};
    int child_size = 0;

    static TurnAction move(Action action) { return {ActionKind::Move, action, 0}; }
    static TurnAction split(int size) { return {ActionKind::Split, {}, size}; }
};

struct Dragon {
    bool alive = false;
    char team = 'A';
    int length = 0;
    Direction facing = Direction::East;
    std::array<Cell, MAX_CELLS> body{};
};

struct Inbox {
    std::array<std::uint64_t, 4> messages{};
    std::size_t count = 0;
};

struct Echoes {
    std::array<int, 5> counts{};

    int count(SonarHit hit) const { return counts[static_cast<int>(hit)]; }
};

struct PortalExit {
    bool present = false;
    Cell cell = 0;
    Direction direction = Direction::North;
};

class World {
    int width_ = 0;
    int height_ = 0;
    std::array<std::uint8_t, MAX_CELLS> kelp_{};
    std::array<int, MAX_CELLS> pearl_countdown_{};
    std::array<int, MAX_CELLS> pearl_reset_{};
    std::array<PortalExit, MAX_CELLS * 4> portals_{};
    bot::bodyguard::core::FixedBitset<MAX_CELLS> pearls_;
    std::array<int, MAX_CELLS> occupant_{};
    std::array<Dragon, MAX_DRAGONS> dragons_{};
    std::array<Inbox, MAX_DRAGONS> inbox_{};
    std::array<Inbox, MAX_DRAGONS> next_round_inbox_{};
    std::array<Echoes, MAX_DRAGONS> echoes_{};
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

    PortalExit portal(Cell source, Direction direction) const {
        return portals_[source * 4 + static_cast<int>(direction)];
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

    static Direction opposite(Direction direction) {
        switch (direction) {
        case Direction::North: return Direction::South;
        case Direction::East: return Direction::West;
        case Direction::South: return Direction::North;
        case Direction::West: return Direction::East;
        }
        return Direction::North;
    }

    static int direction_index(Direction direction) { return static_cast<int>(direction); }

    void enqueue(Inbox& inbox, std::uint64_t message) {
        if (inbox.count < inbox.messages.size())
            inbox.messages[inbox.count++] = message;
    }

    void cast_sonar(int sender_id, Direction direction, std::uint64_t message) {
        const Dragon& sender = dragons_[sender_id];
        Cell origin = sender.body[0];
        Direction ray_direction = direction;
        if (direction == opposite(sender.facing) && sender.length > 1) {
            origin = sender.body[sender.length - 1];
            ray_direction = direction;
        }
        for (int distance = 0; distance < width_ + height_; ++distance) {
            if ((kelp_[origin] & edge_bit(ray_direction)) != 0) {
                ++echoes_[sender_id].counts[static_cast<int>(SonarHit::Kelp)];
                return;
            }
            PortalExit exit = portal(origin, ray_direction);
            Cell destination = exit.present ? next_cell(exit.cell, exit.direction) : next_cell(origin, ray_direction);
            int target_id = occupant_[destination];
            if (target_id >= 0) {
                const Dragon& target = dragons_[target_id];
                bool ally = target.team == sender.team;
                SonarHit hit = ally ? (target.body[0] == destination ? SonarHit::AllyHead : SonarHit::Ally)
                                    : (target.body[0] == destination ? SonarHit::EnemyHead : SonarHit::Enemy);
                ++echoes_[sender_id].counts[static_cast<int>(hit)];
                if (target_id > sender_id)
                    enqueue(inbox_[target_id], message);
                else
                    enqueue(next_round_inbox_[target_id], message);
                return;
            }
            origin = destination;
        }
    }

  public:
    void initialize(int width, int height) {
        width_ = width;
        height_ = height;
        kelp_.fill(0);
        pearl_countdown_.fill(-1);
        pearl_reset_.fill(-1);
        portals_.fill({});
        pearls_.clear();
        occupant_.fill(-1);
        dragons_.fill({});
        inbox_.fill({});
        next_round_inbox_.fill({});
        echoes_.fill({});
        next_id_ = 0;
    }

    int width() const { return width_; }
    int height() const { return height_; }
    Cell cell_at(int x, int y) const { return cell(x, y); }
    Cell head(int dragon_id) const { return dragons_[dragon_id].body[0]; }
    Cell tail(int dragon_id) const { return dragons_[dragon_id].body[dragons_[dragon_id].length - 1]; }
    int length(int dragon_id) const { return dragons_[dragon_id].length; }
    bool alive(int dragon_id) const { return dragons_[dragon_id].alive; }
    int next_unused_id() const { return next_id_; }
    bool has_pearl(Cell position) const { return pearls_.test(static_cast<std::size_t>(position)); }
    const Inbox& inbox(int dragon_id) const { return inbox_[dragon_id]; }
    const Echoes& echoes(int dragon_id) const { return echoes_[dragon_id]; }

    std::uint64_t state_hash() const {
        std::uint64_t hash = 1469598103934665603ULL;
        auto add = [&hash](std::uint64_t value) {
            hash ^= value;
            hash *= 1099511628211ULL;
        };
        add(static_cast<std::uint64_t>(width_));
        add(static_cast<std::uint64_t>(height_));
        for (Cell position = 0; position < width_ * height_; ++position)
            add(has_pearl(position) ? 1U : 0U);
        for (int dragon_id = 0; dragon_id < next_id_; ++dragon_id) {
            const Dragon& dragon = dragons_[dragon_id];
            add(dragon.alive ? 1U : 0U);
            add(static_cast<std::uint64_t>(dragon.length));
            for (int index = 0; index < dragon.length; ++index)
                add(static_cast<std::uint64_t>(dragon.body[index]));
        }
        return hash;
    }
    DeathReason last_death = DeathReason::None;

    void add_dragon(int dragon_id, const std::array<Cell, MAX_CELLS>& body, int length, char team = 'A') {
        dragons_[dragon_id].alive = true;
        dragons_[dragon_id].team = team;
        dragons_[dragon_id].length = length;
        dragons_[dragon_id].body = body;
        if (dragon_id >= next_id_)
            next_id_ = dragon_id + 1;
        rebuild_occupancy();
    }

    void add_dragon(int dragon_id, Cell head_position, char team = 'A') {
        std::array<Cell, MAX_CELLS> body{};
        body[0] = head_position;
        add_dragon(dragon_id, body, 1, team);
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

    void set_pearl_countdown(Cell position, int countdown, int reset_countdown = -1) {
        pearl_countdown_[position] = countdown;
        pearl_reset_[position] = reset_countdown;
    }

    void set_portal(Cell first, Direction first_direction, Cell second, Direction second_direction) {
        portals_[first * 4 + static_cast<int>(first_direction)] = {true, second, second_direction};
        portals_[second * 4 + static_cast<int>(second_direction)] = {true, first, first_direction};
    }

    void tick_pearls() {
        for (Cell position = 0; position < width_ * height_; ++position) {
            if (pearl_countdown_[position] < 0)
                continue;
            if (pearl_countdown_[position] > 0)
                --pearl_countdown_[position];
            if (pearl_countdown_[position] != 0)
                continue;
            if (occupant_[position] < 0 && !has_pearl(position))
                set_pearl(position);
            pearl_countdown_[position] = pearl_reset_[position];
        }
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
            Direction direction = action.steps[step];
            Cell destination = next_cell(dragon.body[0], direction);
            PortalExit exit = portal(dragon.body[0], direction);
            if (exit.present)
                destination = next_cell(exit.cell, exit.direction);
            if (!exit.present && (kelp_[dragon.body[0]] & edge_bit(direction)) != 0)
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
            // Standard body-shift (tail advances, segments shift).
            std::array<Cell, MAX_CELLS> old = dragon.body;
            dragon.body[0] = destination;
            for (int index = 1; index < new_length - 1; ++index)
                dragon.body[index] = old[index - 1];
            if (new_length > 1)
                dragon.body[new_length - 1] = old[new_length - 2];
            dragon.facing = direction;
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

    void run_round(const std::array<TurnAction, MAX_DRAGONS>& actions) {
        tick_pearls();
        inbox_.fill({});
        for (int dragon_id = 0; dragon_id < MAX_DRAGONS; ++dragon_id)
            inbox_[dragon_id] = next_round_inbox_[dragon_id];
        next_round_inbox_.fill({});
        for (int dragon_id = 0; dragon_id < next_id_; ++dragon_id) {
            if (!dragons_[dragon_id].alive)
                continue;
            const TurnAction& action = actions[dragon_id];
            if (action.kind == ActionKind::Split)
                split(dragon_id, action.child_size);
            else if (action.kind == ActionKind::Move)
                move(dragon_id, action.movement);
            else
                kill(dragon_id, DeathReason::NoValidAction);
            if (!dragons_[dragon_id].alive)
                continue;
            for (Direction direction : {Direction::North, Direction::East, Direction::South, Direction::West}) {
                int index = direction_index(direction);
                if (action.kind == ActionKind::Move && action.movement.sonar_set[index])
                    cast_sonar(dragon_id, direction, action.movement.sonar_messages[index]);
            }
        }
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
