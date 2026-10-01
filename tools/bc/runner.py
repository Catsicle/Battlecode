from __future__ import annotations

import json
import os
import subprocess
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from .deaths import classify_events
from .gitinfo import bot_hash, dirty, git_sha, python_stamp, runner_version
from .parse import parse_deaths, parse_game_output, parse_points
from .registry import Bot, discover, select
from .replay import load, unavailable


def schedule(opponents: list[Bot], maps: list[str], seeds: list[int], repeats: int) -> list[dict[str, Any]]:
    games = []
    for opponent in opponents:
        for map_path in maps:
            for seed in seeds:
                for repeat in range(repeats):
                    for side in ("A", "B"):
                        games.append({"opponent": opponent, "map": map_path, "seed": seed, "repeat": repeat, "candidate_team": side})
    return games


def _decode(value: object) -> str:
    return value.decode("utf-8", errors="replace") if isinstance(value, bytes) else str(value or "")


def _command(runner: str, map_path: str, candidate: Bot, opponent: Bot, side: str, seed: int, replay: Path, sandbox: bool) -> list[str]:
    bots = (candidate, opponent) if side == "A" else (opponent, candidate)
    command = [runner, "run", map_path, str(bots[0].path), str(bots[1].path), "--seed", str(seed), "--no-logs", "--no-indicator", "--no-draw", "--output", str(replay)]
    if sandbox:
        command.append("--sandbox")
    return command


def _metrics(points: dict, replay_data: dict) -> dict[str, Any]:
    metrics = {key: unavailable("replay schema is not verified") for key in ("longest_dragon_end", "dragons_end", "pearls_collected", "pearls_total", "no_legal_move_turns", "stuck_loop_turns", "loops_count", "loops", "loop_len_mean", "loop_len_max")}
    metrics["cpu"] = points
    if replay_data.get("available"):
        metrics["replay"] = "loaded but no versioned metric adapter is configured"
    return metrics


def run_one(root: Path, run_dir: Path, candidate: Bot, opponent: Bot, item: dict[str, Any], runner: str, timeout: float, sandbox: bool, keep_output: bool, keep_replays: bool, rules: Path | None) -> dict[str, Any]:
    index = item["index"]
    replay_path = run_dir / "replays" / f"{index:05d}-{candidate.id}-vs-{opponent.id}-{item['candidate_team']}-{item['seed']}.replay"
    replay_path.parent.mkdir(exist_ok=True)
    command = _command(runner, item["map"], candidate, opponent, item["candidate_team"], item["seed"], replay_path, sandbox)
    started = time.monotonic()
    stdout = stderr = ""
    returncode = 0
    reason = None
    if item.get("dry_run"):
        status, winner, rounds, reason = "dry-run", None, None, "dry run"
    else:
        try:
            completed = subprocess.run(command, cwd=root, capture_output=True, timeout=timeout, check=False)
            stdout, stderr, returncode = _decode(completed.stdout), _decode(completed.stderr), completed.returncode
            status, winner, rounds, reason = parse_game_output(stdout + "\n" + stderr)
            if returncode and status not in {"win", "draw"}:
                status, reason = "error", f"runner exited with code {returncode}"
            if status == "win":
                status = "win" if winner == item["candidate_team"] else "loss"
        except subprocess.TimeoutExpired as exc:
            stdout, stderr = _decode(exc.stdout), _decode(exc.stderr)
            status, winner, rounds, reason, returncode = "error", None, None, f"timeout after {timeout} seconds", -1
    deaths = parse_deaths(stdout + "\n" + stderr)
    replay_info = load(replay_path if replay_path.exists() else None)
    row = {"index": index, "candidate": candidate.id, "opponent": opponent.id, "map": item["map"], "seed": item["seed"], "candidate_team": item["candidate_team"], "status": status, "winner": winner, "rounds": rounds, "end_reason": reason, "returncode": returncode, "duration_seconds": round(time.monotonic() - started, 4), "points": parse_points(stdout + "\n" + stderr), "deaths": classify_events(deaths), "metrics": _metrics(parse_points(stdout + "\n" + stderr), replay_info), "command": command, "stdout": stdout if keep_output else "", "stderr": stderr if keep_output else "", "replay": str(replay_path.relative_to(run_dir)) if keep_replays and replay_path.exists() else None, "git_sha": git_sha(root), "dirty": dirty(root), "bot_hash": {candidate.id: candidate.hash, opponent.id: opponent.hash}, "python": python_stamp(), "unswbc_version": runner_version(runner)}
    if not keep_replays and replay_path.exists():
        replay_path.unlink()
    return row


def _append(path: Path, row: dict[str, Any], lock: threading.Lock) -> None:
    with lock, path.open("a", encoding="utf-8") as stream:
        stream.write(json.dumps(row, sort_keys=True) + "\n")
        stream.flush()
        os.fsync(stream.fileno())


def run(root: Path, candidate: Bot, opponents: list[Bot], maps: list[str], seeds: list[int], repeats: int = 1, jobs: int = 1, sandbox: bool = True, timeout: float = 120, out: Path | None = None, runner: str = "unswbc", keep_output: bool = False, keep_replays: bool = False, dry_run: bool = False, resume: Path | None = None) -> Path:
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    run_dir = resume or (out or root / "results" / "runs" / f"{timestamp}-{git_sha(root)}" )
    run_dir.mkdir(parents=True, exist_ok=True)
    jsonl = run_dir / "games.jsonl"
    existing = {json.loads(line)["index"] for line in jsonl.read_text(encoding="utf-8").splitlines() if line.strip()} if jsonl.exists() else set()
    items = schedule(opponents, maps, seeds, repeats)
    items = [{**item, "index": index + 1, "dry_run": dry_run} for index, item in enumerate(items) if index + 1 not in existing]
    lock = threading.Lock()
    with ThreadPoolExecutor(max_workers=max(1, jobs)) as pool:
        futures = [pool.submit(run_one, root, run_dir, candidate, item["opponent"], item, runner, timeout, sandbox, keep_output, keep_replays, root / "config" / "death_rules.toml") for item in items]
        for position, future in enumerate(as_completed(futures), 1):
            row = future.result()
            _append(jsonl, row, lock)
            print(f"[{len(existing) + position}/{len(existing) + len(items)}] {row['candidate']} vs {row['opponent']} | {row['status'].upper()} r={row.get('rounds') or '-'}")
    return run_dir
