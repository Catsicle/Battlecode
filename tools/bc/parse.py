from __future__ import annotations

import re
from typing import Any

WIN_RE = re.compile(r"team ([AB]) wins after (\d+) rounds(?: \(([^)]+)\))?")
DRAW_RE = re.compile(r"draw after (\d+) rounds(?: \(([^)]+)\))?")
POINTS_RE = re.compile(r"team ([AB]) points per turn:\s+p50\s+([\d.]+M)\s+p99\s+([\d.]+M)\s+mean\s+([\d.]+M)\s+max\s+([\d.]+M)\s+\((\d+) turns\)")
DEATH_RE = re.compile(r"round (\d+): bot (\d+) \(team ([AB])\) died: (.+?)\s*$")


def _text(value: object) -> str:
    if isinstance(value, bytes):
        return value.decode("utf-8", errors="replace")
    return str(value or "")


def parse_game_output(text: object) -> tuple[str, str | None, int | None, str | None]:
    text = _text(text)
    match = WIN_RE.search(text)
    if match:
        team, rounds, reason = match.groups()
        return "win", team, int(rounds), reason
    match = DRAW_RE.search(text)
    if match:
        rounds, reason = match.groups()
        return "draw", None, int(rounds), reason
    return "error", None, None, "runner produced no game result"


def millions(value: str) -> float:
    return float(value.rstrip("M"))


def parse_points(text: object) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    for match in POINTS_RE.finditer(_text(text)):
        team, p50, p99, avg, maximum, turns = match.groups()
        result[team] = {"p50_M": millions(p50), "p99_M": millions(p99), "mean_M": millions(avg), "max_M": millions(maximum), "turns": int(turns)}
    return result


def parse_deaths(text: object) -> list[dict[str, Any]]:
    return [{"round": int(rounds), "bot_id": int(bot), "team": team, "cause": cause.strip()} for rounds, bot, team, cause in DEATH_RE.findall(_text(text))]
