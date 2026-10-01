from __future__ import annotations

from typing import Any

from .replay import unavailable


REPLAY_METRICS = ("longest_dragon_end", "dragons_end", "pearls_collected", "pearls_total", "no_legal_move_turns", "stuck_loop_turns", "loops_count", "loops", "loop_len_mean", "loop_len_max")


def unavailable_metrics(reason: str = "replay schema is not verified") -> dict[str, Any]:
    return {name: unavailable(reason) for name in REPLAY_METRICS}


def from_game(points: dict[str, Any], replay: dict[str, Any]) -> dict[str, Any]:
    result = unavailable_metrics()
    result["cpu_p50_M"] = {team: value.get("p50_M") for team, value in points.items() if isinstance(value, dict)}
    result["cpu_p99_M"] = {team: value.get("p99_M") for team, value in points.items() if isinstance(value, dict)}
    result["replay_available"] = bool(replay.get("available"))
    return result
