from __future__ import annotations

import json
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path

from .gitinfo import bot_hash, dirty, git_sha


def create(root: Path, name: str, note: str = "", force: bool = False) -> Path:
    if dirty(root / "src") and not force:
        raise RuntimeError("src has uncommitted changes; use --force to snapshot")
    destination = root / "bots" / "history" / name
    if destination.exists():
        raise RuntimeError(f"snapshot already exists: {name}")
    shutil.copytree(root / "src", destination)
    (destination / "SNAPSHOT.json").write_text(json.dumps({"name": name, "git_sha": git_sha(root), "bot_hash": bot_hash(destination), "date": datetime.now(timezone.utc).isoformat(), "note": note}, indent=2), encoding="utf-8")
    subprocess.run(["git", "add", str(destination)], cwd=root, check=True)
    subprocess.run(["git", "commit", "-m", f"snapshot {name}"], cwd=root, check=True)
    subprocess.run(["git", "tag", name], cwd=root, check=True)
    return destination
