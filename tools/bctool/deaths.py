from __future__ import annotations

import re
from pathlib import Path


DEFAULT_RULES = {
    "suicide": [r"hit itself", r"hit self", r"hit a wall", r"hit wall", r"no valid action", r"timeout", r"crash", r"kelp"],
    "deliberate_attack": [r"head-to-head", r"head to head", r"hit another dragon", r"other body"],
    "other": [r"starv", r"environment"],
}


def load_rules(path: Path | None = None) -> dict[str, list[re.Pattern[str]]]:
    if path is None or not path.exists():
        return {key: [re.compile(pattern, re.I) for pattern in patterns] for key, patterns in DEFAULT_RULES.items()}
    try:
        import tomllib
        data = tomllib.loads(path.read_text(encoding="utf-8"))
        raw = data.get("rules", data)
    except (OSError, ValueError):
        raw = DEFAULT_RULES
    return {str(key): [re.compile(str(pattern), re.I) for pattern in patterns] for key, patterns in raw.items() if isinstance(patterns, list)}


def classify(cause: str, rules: dict[str, list[re.Pattern[str]]] | None = None) -> str:
    for category, patterns in (rules or load_rules()).items():
        if any(pattern.search(cause) for pattern in patterns):
            return category
    return "unclassified"


def classify_events(events: list[dict], rules: dict[str, list[re.Pattern[str]]] | None = None) -> list[dict]:
    return [{**event, "category": classify(str(event.get("cause", "")), rules)} for event in events]
