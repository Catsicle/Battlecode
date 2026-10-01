from __future__ import annotations

import json
from pathlib import Path
from typing import Any


def load(path: Path | None) -> dict[str, Any]:
    if path is None:
        return {"available": False, "reason": "no replay was retained"}
    try:
        raw = path.read_bytes()
    except OSError as exc:
        return {"available": False, "reason": f"replay could not be read: {exc}"}
    try:
        value = json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return {"available": False, "reason": "binary replay schema is not verified; install unswbc for its parser"}
    if not isinstance(value, dict):
        return {"available": False, "reason": "replay JSON root is not an object"}
    return {"available": True, "data": value}


def unavailable(reason: str) -> dict[str, Any]:
    return {"value": None, "reason": reason}
