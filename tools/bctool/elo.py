from __future__ import annotations

import json
from collections import defaultdict
from pathlib import Path


def leaderboard(root: Path) -> list[dict]:
    records = []
    for path in sorted((root / "results" / "runs").glob("*/games.jsonl")):
        records.extend(json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip())
    scores = defaultdict(lambda: [1000.0, 0, 0, 0, 0])
    for row in sorted(records, key=lambda value: value.get("timestamp", "")):
        if row.get("status") not in {"win", "loss", "draw"}:
            continue
        a, b = row["candidate"], row["opponent"]
        sa, sb = scores[a], scores[b]
        expected = 1 / (1 + 10 ** ((sb[0] - sa[0]) / 400))
        actual = 0.5 if row["status"] == "draw" else 1.0
        delta = 32 * (actual - expected)
        sa[0] += delta; sb[0] -= delta
        sa[1] += 1; sb[1] += 1
        if row["status"] == "win": sa[2] += 1; sb[3] += 1
        elif row["status"] == "loss": sa[3] += 1; sb[2] += 1
        else: sa[4] += 1; sb[4] += 1
    return [{"id": name, "rating": round(values[0], 2), "games": values[1], "wins": values[2], "losses": values[3], "draws": values[4]} for name, values in sorted(scores.items(), key=lambda pair: pair[1][0], reverse=True)]


def write(root: Path) -> Path:
    data = leaderboard(root)
    path = root / "results" / "elo_leaderboard.json"
    path.write_text(json.dumps(data, indent=2), encoding="utf-8")
    (root / "results" / "elo_leaderboard.md").write_text("# ELO leaderboard\n\n| Bot | Rating | Games | W-L-D |\n| --- | ---: | ---: | ---: |\n" + "\n".join(f"| {r['id']} | {r['rating']} | {r['games']} | {r['wins']}-{r['losses']}-{r['draws']} |" for r in data) + "\n", encoding="utf-8")
    return path
