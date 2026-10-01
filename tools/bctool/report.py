from __future__ import annotations

import json
from collections import Counter, defaultdict
from pathlib import Path

from .stats import elo_diff, wilson


def rows(run_dir: Path) -> list[dict]:
    path = run_dir / "games.jsonl"
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()] if path.exists() else []


def summarize(data: list[dict]) -> dict:
    counts = Counter(row.get("status") for row in data)
    wins, losses = counts["win"], counts["loss"]
    low, high = wilson(wins, wins + losses)
    return {"games": len(data), "wins": wins, "losses": losses, "draws": counts["draw"], "errors": counts["error"], "decisive_win_rate": wins / (wins + losses) if wins + losses else 0.0, "wilson95": [low, high], "elo_difference": elo_diff(wins / (wins + losses)) if wins + losses and wins not in (0, wins + losses) else None}


def write(run_dir: Path) -> Path:
    data = rows(run_dir)
    summary = summarize(data)
    by_opponent: dict[str, Counter] = defaultdict(Counter)
    by_map: dict[str, Counter] = defaultdict(Counter)
    by_side: dict[str, Counter] = defaultdict(Counter)
    for row in data:
        for bucket, key in ((by_opponent, "opponent"), (by_map, "map"), (by_side, "candidate_team")):
            bucket[row.get(key, "unknown")][row.get("status", "error")] += 1
    lines = ["# Battlecode run report", "", f"Games: {summary['games']}", f"W-L-D: {summary['wins']}-{summary['losses']}-{summary['draws']}", f"Errors: {summary['errors']}", f"Decisive win rate: {summary['decisive_win_rate']:.1%}", f"Wilson 95%: {summary['wilson95'][0]:.1%} - {summary['wilson95'][1]:.1%}", f"ELO difference: {summary['elo_difference']}", "", "## By opponent", "", "| Opponent | W | L | D |", "| --- | ---: | ---: | ---: |"]
    for name, counts in sorted(by_opponent.items()):
        lines.append(f"| {name} | {counts['win']} | {counts['loss']} | {counts['draw']} |")
    lines += ["", "## By map", "", "| Map | W | L | D |", "| --- | ---: | ---: | ---: |"]
    for name, counts in sorted(by_map.items()):
        lines.append(f"| {name} | {counts['win']} | {counts['loss']} | {counts['draw']} |")
    lines += ["", "## By side", "", "| Side | W | L | D |", "| --- | ---: | ---: | ---: |"]
    for name, counts in sorted(by_side.items()):
        lines.append(f"| {name} | {counts['win']} | {counts['loss']} | {counts['draw']} |")
    path = run_dir / "report.md"
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    (run_dir / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    return path
