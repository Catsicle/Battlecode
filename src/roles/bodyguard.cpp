#include "bodyguard.hpp"
#include "bodyguard/constants.hpp"
#include "bodyguard/rect_adapter.hpp"
#include "bodyguard/comparator.hpp"
#include "../core/types.hpp"
namespace { using namespace ::bodyguard; }
#include <cassert>

namespace bot::roles {

namespace {

#include <iostream>
#include <limits>
#include <cstdlib>
#ifndef BG_LOG
#define BG_LOG(x) do {} while (0)
#endif

inline int wrap_delta(int d, int N) {
    if (N > 1) { if (d > N/2) d -= N; else if (d < -N/2) d += N; }
    return d;
}
inline StepDir step_dir_between(const Vec2& a, const Vec2& b, int W, int H) {
    int dx = wrap_delta(b.x - a.x, W), dy = wrap_delta(b.y - a.y, H);
    if (dx == 1) return StepDir::Right;
    if (dx == -1) return StepDir::Left;
    if (dy == 1) return StepDir::Down;
    return StepDir::Up;
}
inline StepDir opposite(StepDir d) {
    switch (d) { case StepDir::Right: return StepDir::Left; case StepDir::Left: return StepDir::Right;
                 case StepDir::Down: return StepDir::Up;    default: return StepDir::Down; }
}
inline Vec2 neighbor(Vec2 p, StepDir d, int W, int H) {
    switch (d) { case StepDir::Right: p.x = (p.x+1)%W; break; case StepDir::Left: p.x = (p.x-1+W)%W; break;
                 case StepDir::Down: p.y = (p.y+1)%H; break;  default: p.y = (p.y-1+H)%H; }
    return p;
}

RingPath build_ring_path(const ::bodyguard::rect::Box& b, int W, int H) {
    RingPath path{};
    const int count = 2 * (b.w + b.h) - 4;
    if (b.w < 2 || b.h < 2 || count <= 0 || count > RingPath::MAX_CELLS) return path;
    int x = b.x0, y = b.y0;
    for (int i = 0; i < b.w - 1; ++i) { path.push(Vec2{x, y}); x = (x + 1) % W; }
    for (int i = 0; i < b.h - 1; ++i) { path.push(Vec2{x, y}); y = (y + 1) % H; }
    for (int i = 0; i < b.w - 1; ++i) { path.push(Vec2{x, y}); x = (x - 1 + W) % W; }
    for (int i = 0; i < b.h - 1; ++i) { path.push(Vec2{x, y}); y = (y - 1 + H) % H; }
    for (int i = 0; i < count; ++i)
        path.moves[i] = step_dir_between(path.cells[i], path.cells[(i + 1) % count], W, H);
    return path;
}

struct RingEval { bool valid = false; int pearls = 0, len = 0, score = std::numeric_limits<int>::min(); };

RingEval evaluate_ring(const RingPath& p, const bot::Memory& m, int round,
                       int snake_len, const Vec2& head, int W, int H, int enemy_penalty = 0) {
    RingEval e; e.len = p.count;
    int entry = 1 << 20;
    for (int i = 0; i < p.count; ++i) {
        const int idx = m.index(p.cells[i]);
        const int sp = m.pearl_spawn_round[idx];
        const bool now = m.pearl[idx];
        const bool soon = !now && !m.never_spawns[idx] &&
                          sp != std::numeric_limits<int>::max() &&
                          sp <= round + bodyguard_constants::PEARL_HORIZON;
        if (now || soon) ++e.pearls;
        const int d = std::abs(wrap_delta(p.cells[i].x - head.x, W)) +
                      std::abs(wrap_delta(p.cells[i].y - head.y, H));
        entry = std::min(entry, d);
    }
    const int density_pct = 100 * e.pearls / std::max(1, e.len);
    const int len_gap = std::abs((e.len - bodyguard_constants::KEEP_MARGIN) - snake_len);
    e.score = 2 * density_pct - 3 * len_gap - 2 * entry - 4 * enemy_penalty;
    e.valid = true;
    return e;
}

inline int plan_split(int len, int growth, int count) {
    using namespace bodyguard_constants;
    const int projected = len + growth;
    if (projected < count - SPLIT_TRIGGER_MARGIN) return 0;
    const int child = projected - (count - KEEP_MARGIN);
    return (child >= MIN_CHILD && child < len) ? child : 0;
}

} // anonymous namespace

void BodyguardRole::reset() {
    state = BodyguardState::NeedRing;
    path = RingPath{};
    current_index = 0;
}

bool BodyguardRole::build_ring(
    const unswbc::Controller& controller,
    const Memory& memory,
    int round
) {
    static ::bodyguard::rect::Snapshot snap;   // NOTE: shared across instances
    ::bodyguard::rect_adapter::fill_snapshot(memory, round, snap);
    if (snap.W == 0) return false;

    const Vec2 head = controller.get_position();
    const int len_now = controller.get_length();
    constexpr int K = 5;
    ::bodyguard::rect::Box out[K];
    const int n = ::bodyguard::rect_adapter::query_top_boxes(snap, head.x, head.y, 128, K, out);

    if (search_start_round < 0) search_start_round = round;
    const int Lq = len_now;                       // validate with the real length, not DEFAULT_LQ
    const int steps = bodyguard_constants::compute_steps(Lq);
    ::bodyguard::rect::Plan plan{};

    RingPath best_path{}; RingEval best{};
    for (int i = 0; i < n; ++i) {
        if (::bodyguard::rect::validate(snap, out[i], head.x, head.y, Lq, steps, plan)
            != ::bodyguard::rect::Verdict::OK) continue;
        if ((out[i].w - 2) * (out[i].h - 2) < bodyguard_constants::MIN_INTERIOR_AREA) continue;
        RingPath cand = build_ring_path(out[i], snap.W, snap.H);
        if (cand.count == 0 || cand.count > bodyguard_constants::MAX_CYCLE_LENGTH) continue;
        RingEval e = evaluate_ring(cand, memory, round, len_now, head, snap.W, snap.H);
        if (e.score > best.score) { best = e; best_path = cand; }
    }
    if (!best.valid) { has_pending = false; return false; }

    // hysteresis: don't abandon a committed ring for a marginal gain
    if (path.count > 0 && best.score < ring_score + bodyguard_constants::SWITCH_HYSTERESIS) return true;

    const bool good = best.pearls * 100 >= best.len * bodyguard_constants::MIN_PEARL_PCT;
    const bool patience_over = round - search_start_round >= bodyguard_constants::SEARCH_PATIENCE;
    if (good || patience_over) {
        path = best_path; ring_score = best.score; search_start_round = -1; has_pending = false;
        return true;
    }
    // Not committed yet: pre-position instead of idling.
    has_pending = true;
    pending_target = best_path.cells[0];
    return false;
}

Action BodyguardRole::act(const unswbc::Controller& controller, const Memory& memory,
                          const nav::Grid& grid, const comms::Inbox& inbox) {

    Action action{};
    const Vec2 head = controller.get_position();

    if (state == BodyguardState::NeedRing) {
        if (!build_ring(controller, memory, memory.current_round)) {
            if (has_pending) {                     // pre-position, never idle
                action.request.mode = NavMode::Navigate;
                action.request.has_target = true;
                action.request.target = pending_target;
            }
            return action;
        }
        state = BodyguardState::EnteringRing;
        dir_sign = 1;
    }

    if (state == BodyguardState::EnteringRing) {
        int idx = locate(head);
        if (idx >= 0) { current_index = idx; state = BodyguardState::Tracing; }
        else {
            // go to the NEAREST ring cell, not cells[0]
            int bestd = 1 << 20; Vec2 tgt = path.cells[0];
            for (int i = 0; i < path.count; ++i) {
                int d = std::abs(wrap_delta(path.cells[i].x - head.x, memory.width)) +
                        std::abs(wrap_delta(path.cells[i].y - head.y, memory.height));
                if (d < bestd) { bestd = d; tgt = path.cells[i]; }
            }
            action.request.mode = NavMode::Navigate;
            action.request.has_target = true;
            action.request.target = tgt;
            return action;
        }
    }

    if (state == BodyguardState::Tracing) {
        action.request.mode = NavMode::Direct;
        action.request.direct_direction = path.moves[current_index];
        return action;
    }
    if (state == BodyguardState::Dying) { return action; }
    return action;



Role& bodyguard_role() {
    static BodyguardRole role;
    return role;
}

int BodyguardRole::locate(const Vec2& p) const {
    for (int i = 0; i < path.count; ++i) if (same_position(p, path.cells[i])) return i;
    return -1;
}

Action BodyguardRole::die(const unswbc::Controller& controller, const Memory& memory, const nav::Grid&, const comms::Inbox&) {
    Action action{};
    const Vec2 head = controller.get_position();
    const auto& body = memory.own_body;
    const StepDir dirs[] = {StepDir::Up, StepDir::Down, StepDir::Left, StepDir::Right};
    for (StepDir d : dirs) {
        Vec2 c = neighbor(head, d, memory.width, memory.height);
        for (size_t k = 2; k < body.size(); ++k)
            if (body[k].x == c.x && body[k].y == c.y) {
                action.request.mode = NavMode::Direct;
                action.request.has_target = false;
                action.request.direct_direction = d;
                return action;
            }
    }
    state = BodyguardState::NeedRing;
    return action;
}

} // namespace bot::roles
