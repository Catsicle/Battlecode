#include "role.hpp"
#include "../comms/use_sonar.hpp"
#include "../config.hpp"
#include "killer/kill.hpp"
#include <array>
#include <utility>

namespace bot::roles {

namespace {

constexpr int GUARD_ENTER_LEN  = 7;  // start guarding at this length
constexpr int GUARD_EXIT_LEN   = 5;   // hysteresis: go back to feeding below this
constexpr int GUARD_TARGET_LEN = 7;  // split down to this
constexpr int GUARD_MAX_LEN    = 9;  // split when longer than this
constexpr int LEASH            = 6;   // intercept enemy heads this close to the queen
constexpr int BEACON_STALE     = 15;  // ignore beacons older than this many rounds
constexpr int LEAD             = 2;   // aim this many ring tiles ahead

// Chebyshev radius-2 ring, in contiguous order (16 tiles)
constexpr std::array<std::pair<int, int>, 16> RING = {{
    {2,-2},{2,-1},{2,0},{2,1},{2,2},{1,2},{0,2},{-1,2},
    {-2,2},{-2,1},{-2,0},{-2,-1},{-2,-2},{-1,-2},{0,-2},{1,-2}}};

int wrap(int v, int m) { return (v % m + m) % m; }
int tdelta(int a, int b, int m) {            // shortest signed a-b on a torus
    int d = wrap(a - b, m);
    return d > m / 2 ? d - m : d;
}
int cheb(int dx, int dy) { return std::max(std::abs(dx), std::abs(dy)); }

} // namespace

class BodyguardRole final : public Role {
  private:
    bool guarding = false;
    int spin = 1;
    int last_flip_round = 0;
    Vec2 last_queen{};
    int last_queen_round = -1000000;

  public:
    Action act(const unswbc::Controller& controller, const Memory& memory,
               const nav::Grid& grid, const comms::Inbox& inbox,
               const bot::BodyTracker& tracker) override {
        (void)tracker;
        Action action;
        const int round = grid.round;
        const Vec2 head = controller.get_position();
        const int len = controller.get_length();
        const int W = memory.width, H = memory.height;

        if (inbox.suicide_exists) {
            action.request.mode = NavMode::Suicide;
            return action;
        }

        // Relay enemy-queen sightings, same as the other roles
        if (auto s = comms::maybe_emit_enemy_queen_sonar(controller, inbox, action.round_num)) {
            action.sonar.North = action.sonar.East = action.sonar.South = action.sonar.West = s;
        }

        // Beacon emission for bodyguard coordination
        std::uint64_t beacon_sonar = bot::comms::maybe_emit_queen_beacon_sonar(
            controller, inbox, action.round_num);
        printf("DEBUG: bodyguard beacon_sonar=%lu\n", (unsigned long)beacon_sonar);
        if (beacon_sonar != 0) {
            printf("DEBUG: bodyguard emitting beacon sonar\n");
            action.sonar.North = beacon_sonar;
            action.sonar.East = beacon_sonar;
            action.sonar.South = beacon_sonar;
            action.sonar.West = beacon_sonar;
        }

        // ---- 1. Locate the queen: vision (exact) beats beacon (stale) ----
        bool queen_visible = false;
        for (const auto& tile : controller.get_vision().get_tiles()) {
            const auto* d = tile.get_dragon();
            if (d && d->is_head() && (d->get_id() == 0 || d->get_id() == 1) &&
                d->get_team() == controller.get_team()) {
                last_queen = tile.get_position();
                last_queen_round = round;
                queen_visible = true;
                break;
            }
        }
        if (!queen_visible && inbox.queen_beacon_exists) {          // NEW inbox field
            for (const auto& m : inbox.queen_beacon_messages) {
                if ((int)m.round > last_queen_round) {
                    last_queen = {(int)m.x, (int)m.y};
                    last_queen_round = (int)m.round;
                }
            }
        }
        const bool queen_known = (round - last_queen_round) <= BEACON_STALE;
        printf("DEBUG: bodyguard job=queen_location visible=%d beacon_used=%d known_round=%d stale=%d queen_known=%d\n",
               queen_visible ? 1 : 0,
               (!queen_visible && inbox.queen_beacon_exists) ? 1 : 0,
               last_queen_round, BEACON_STALE, queen_known ? 1 : 0);

        // ---- 2. Feeding / guard state with hysteresis ----
        printf("DEBUG: bodyguard job=guard_state len=%d guarding=%s queen_known=%s beacon_exists=%d beacon_msgs=%zu\n",
               len, guarding ? "true" : "false", queen_known ? "true" : "false",
               (int)inbox.queen_beacon_exists, inbox.queen_beacon_messages.size());
        if (!guarding && len >= GUARD_ENTER_LEN && queen_known) {
            printf("DEBUG: bodyguard job=enter_guard len=%d\n", len);
            guarding = true;
        }
        if (guarding && (len < GUARD_EXIT_LEN)) {
            printf("DEBUG: bodyguard job=exit_guard len=%d\n", len);
            guarding = false;
        }

        if (!guarding) {
            action.request.mode = NavMode::Explore;       // eat
            return action;
        }

        // ---- 3. Trim length so the head can't bite its own ring ----
        printf("DEBUG: bodyguard job=check_split len=%d max=%d target=%d can_split=%d\n",
               len, GUARD_MAX_LEN, GUARD_TARGET_LEN, controller.can_split(len - GUARD_TARGET_LEN) ? 1 : 0);
        if (len > GUARD_MAX_LEN && controller.can_split(len - GUARD_TARGET_LEN)) {
            printf("DEBUG: bodyguard job=split action split_size=%d\n", len - GUARD_TARGET_LEN);
            action.do_split = true;
            action.split_size = len - GUARD_TARGET_LEN;
            return action;
        }

        if (!queen_known) {                               // lost her: search
            action.request.mode = NavMode::Explore;
            return action;
        }

        // ---- 4. Intercept enemy heads inside the leash ----
        printf("DEBUG: bodyguard job=intercept_check leash=%d\n", LEASH);
        for (const auto& tile : controller.get_vision().get_tiles()) {
            const auto* d = tile.get_dragon();
            if (!d || !d->is_head() || d->get_team() == controller.get_team()) continue;
            Vec2 p = tile.get_position();
            if (cheb(tdelta(p.x, last_queen.x, W), tdelta(p.y, last_queen.y, H)) > LEASH) continue;
            bot::KillResult kr = bot::kill(controller.get_vision(), p, len);
            printf("DEBUG: bodyguard job=intercept_result action=%d path_len=%zu\n",
                   (int)kr.action, kr.path.size());
            if (kr.action == bot::KillAction::KILL_QUEEN ||
                kr.action == bot::KillAction::KILL_ENEMY_HEAD) {
                action.request.mode = NavMode::Direct;
                action.request.path.clear();
                for (char c : kr.path) action.request.path.push_back(Dir(c));
                return action;
            }
        }

        // ---- 5. Occasionally reverse spin (anti-timing) ----
        if (round - last_flip_round > 12 && ((round * 2654435761u) >> 27) % 6 == 0) {
            spin = -spin;
            last_flip_round = round;
        }

        // ---- 6. Orbit: nearest ring tile, then lead along the spin ----
        const int rx = tdelta(head.x, last_queen.x, W);
        const int ry = tdelta(head.y, last_queen.y, H);
        int nearest = 0, best = 1 << 30;
        for (int i = 0; i < 16; ++i) {
            int dx = RING[i].first - rx, dy = RING[i].second - ry;
            int d2 = dx * dx + dy * dy;
            if (d2 < best) { best = d2; nearest = i; }
        }
        printf("DEBUG: bodyguard job=orbit nearest_idx=%d spin=%d\n", nearest, spin);
        for (int lead = LEAD; lead >= 0; --lead) {       // fall back to shorter leads
            int idx = wrap(nearest + spin * lead, 16);
            Vec2 t{wrap(last_queen.x + RING[idx].first, W),
                   wrap(last_queen.y + RING[idx].second, H)};
            int cell = memory.index(t);
            printf("DEBUG: bodyguard job=orbit lead=%d idx=%d target=(%d,%d) cell=%d known=%d fresh=%d present=%d\n",
                   lead, idx,
                   wrap(last_queen.x + RING[idx].first, W),
                   wrap(last_queen.y + RING[idx].second, H),
                   cell, memory.known(cell) ? 1 : 0,
                   memory.occ_fresh(cell, round) ? 1 : 0,
                   memory.occupancy[cell].present ? 1 : 0);
            if (cell >= 0 && memory.known(cell) && memory.kelp[cell] == 0x0F) continue;
            if (memory.occ_fresh(cell, round) && memory.occupancy[cell].present) continue;
            action.request.mode = NavMode::Navigate;
            action.request.has_target = true;
            action.request.target = t;
            return action;
        }
        action.request.mode = NavMode::Explore;
        return action;
    }
};

Role& bodyguard_role() {
    static BodyguardRole role;
    return role;
}

} // namespace bot::roles