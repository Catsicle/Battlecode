from __future__ import annotations

import hashlib
import subprocess
import sys
from pathlib import Path


def _git(root: Path, *args: str) -> str | None:
    try:
        return subprocess.check_output(["git", *args], cwd=root, text=True, stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def git_sha(root: Path) -> str:
    return (_git(root, "rev-parse", "--short", "HEAD") or "nogit")


def dirty(root: Path) -> bool:
    return bool(_git(root, "status", "--porcelain"))


def bot_hash(bot_dir: Path) -> str:
    digest = hashlib.sha256()
    paths = sorted(path for path in bot_dir.rglob("*") if path.is_file() and path.suffix.lower() in {".cpp", ".hpp", ".h", ".c", ".py", ".toml"})
    for path in paths:
        digest.update(path.relative_to(bot_dir).as_posix().encode())
        digest.update(b"\0")
        digest.update(path.read_bytes().replace(b"\r\n", b"\n"))
        digest.update(b"\0")
    return digest.hexdigest()[:10]


def python_stamp() -> str:
    return sys.version.split()[0]


def runner_version(runner: str) -> str | None:
    try:
        return subprocess.check_output([runner, "--version"], text=True, stderr=subprocess.STDOUT, timeout=10).strip()
    except (OSError, subprocess.SubprocessError):
        return None
