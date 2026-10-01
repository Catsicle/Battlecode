from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .gitinfo import bot_hash


@dataclass(frozen=True)
class Bot:
    id: str
    group: str
    path: Path
    hash: str


def _is_bot(path: Path) -> bool:
    return (path / "bot.toml").is_file() and any((path / name).is_file() for name in ("main.cpp", "main.cc", "main.c", "main.py", "main.wasm"))


def discover(root: Path) -> list[Bot]:
    found: list[Bot] = []
    active = root / "src"
    if _is_bot(active):
        found.append(Bot("active", "active", active, bot_hash(active)))
    for group in ("baselines", "opponents", "history"):
        base = root / "bots" / group
        if base.is_dir():
            for marker in sorted(base.rglob("bot.toml")):
                path = marker.parent
                if _is_bot(path):
                    relative = path.relative_to(base)
                    bot_id = path.name if len(relative.parts) == 1 else "/".join(relative.parts)
                    found.append(Bot(bot_id, group, path, bot_hash(path)))
    return found


def select(bots: list[Bot], selectors: str | list[str] | None, exclude: str | list[str] | None = None) -> list[Bot]:
    wanted = [item.strip() for item in ([selectors] if isinstance(selectors, str) else (selectors or [])) for item in item.split(",") if item.strip()]
    excluded = {item.strip() for item in ([exclude] if isinstance(exclude, str) else (exclude or [])) for item in item.split(",") if item.strip()}
    if not wanted or "all" in wanted:
        result = list(bots)
    else:
        result = [bot for bot in bots if bot.id in wanted or bot.group in wanted]
    return [bot for bot in result if bot.id not in excluded]
