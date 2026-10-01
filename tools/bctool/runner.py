from __future__ import annotations

import json
import os
import queue
import subprocess
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from .deaths import classify_events, load_rules
from .gitinfo import bot_hash, dirty, git_sha, python_stamp, runner_version
from .metrics import analyze_replay, unavailable_metrics
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


def _command(runner: str, map_path: str, candidate: Bot, opponent: Bot, side: str, seed: int, replay: Path, sandbox: bool, verbose: bool = False) -> list[str]:
    bots = (candidate, opponent) if side == "A" else (opponent, candidate)
    command = [runner, "run", map_path, str(bots[0].path), str(bots[1].path), "--seed", str(seed), "--no-logs", "--no-indicator", "--no-draw", "--output", str(replay)]
    if sandbox:
        command.append("--sandbox")
    if verbose:
        command.append("-v")
    return command


def _metrics(points: dict, replay_data: dict, rules: dict[str, Any] | None = None) -> dict[str, Any]:
    metrics = analyze_replay(replay_data["data"], rules) if replay_data.get("available") else {"teams": {"A": unavailable_metrics(), "B": unavailable_metrics()}, "curves": {}, "unclassified_death_causes": []}
    metrics["cpu"] = points
    for team in "AB":
        team_points = points.get(team, {})
        metrics["teams"].setdefault(team, {})["cpu_p50_M"] = team_points.get("p50_M")
        metrics["teams"][team]["cpu_p99_M"] = team_points.get("p99_M")
        metrics["teams"][team]["cpu_mean_M"] = team_points.get("mean_M")
        metrics["teams"][team]["cpu_max_M"] = team_points.get("max_M")
    if not replay_data.get("available"):
        metrics["replay"] = replay_data.get("reason", "replay unavailable")
    return metrics


def _stream_process(command: list[str], root: Path, timeout: float, index: int) -> tuple[str, int, bool]:
    process = subprocess.Popen(command, cwd=root, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
    output: list[str] = []
    lines: queue.Queue[str | None] = queue.Queue()

    def read_output() -> None:
        assert process.stdout is not None
        for line in process.stdout:
            lines.put(line)
        lines.put(None)

    reader = threading.Thread(target=read_output, daemon=True)
    reader.start()
    ended = False
    deadline = time.monotonic() + timeout
    while not ended:
        try:
            line = lines.get(timeout=0.1)
        except queue.Empty:
            if time.monotonic() >= deadline:
                process.kill()
                process.wait()
                reader.join(timeout=1)
                while True:
                    try:
                        line = lines.get_nowait()
                    except queue.Empty:
                        break
                    if line is not None:
                        output.append(line)
                return "".join(output), -1, True
            continue
        if line is None:
            ended = True
        else:
            output.append(line)
            print(f"[game {index}] {line.rstrip()}", flush=True)
    return "".join(output), process.wait(), False


def run_one(root: Path, run_dir: Path, candidate: Bot, opponent: Bot, item: dict[str, Any], runner: str, timeout: float, sandbox: bool, keep_output: bool, keep_replays: bool, rules: Path | None, verbose: bool = False) -> dict[str, Any]:
    index = item["index"]
    replay_path = run_dir / "replays" / f"{index:05d}-{candidate.id}-vs-{opponent.id}-{item['candidate_team']}-{item['seed']}.replay"
    replay_path.parent.mkdir(exist_ok=True)
    command = _command(runner, item["map"], candidate, opponent, item["candidate_team"], item["seed"], replay_path, sandbox, verbose)
    started = time.monotonic()
    stdout = stderr = ""
    returncode = 0
    reason = None
    if item.get("dry_run"):
        status, winner, rounds, reason = "dry-run", None, None, "dry run"
    else:
        try:
            if verbose:
                streamed, returncode, timed_out = _stream_process(command, root, timeout, index)
                stdout, stderr = streamed, ""
                if timed_out:
                    raise subprocess.TimeoutExpired(command, timeout, output=stdout)
            else:
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
        except FileNotFoundError as exc:
            status, winner, rounds, reason, returncode = "error", None, None, f"runner not found: {exc.filename}", -1
    death_rules = load_rules(rules)
    deaths = parse_deaths(stdout + "\n" + stderr)
    replay_info = load(replay_path if replay_path.exists() else None)
    points = parse_points(stdout + "\n" + stderr)
    row = {"index": index, "candidate": candidate.id, "opponent": opponent.id, "map": item["map"], "seed": item["seed"], "candidate_team": item["candidate_team"], "status": status, "winner": winner, "rounds": rounds, "end_reason": reason, "returncode": returncode, "duration_seconds": round(time.monotonic() - started, 4), "points": points, "deaths": classify_events(deaths, death_rules), "metrics": _metrics(points, replay_info, death_rules), "command": command, "stdout": stdout if keep_output else "", "stderr": stderr if keep_output else "", "replay": str(replay_path.relative_to(run_dir)) if keep_replays and replay_path.exists() else None, "git_sha": git_sha(root), "dirty": dirty(root), "bot_hash": {candidate.id: candidate.hash, opponent.id: opponent.hash}, "python": python_stamp(), "unswbc_version": runner_version(runner)}
    if not keep_replays and replay_path.exists():
        replay_path.unlink()
    return row


def _append(path: Path, row: dict[str, Any], lock: threading.Lock) -> None:
    with lock, path.open("a", encoding="utf-8") as stream:
        stream.write(json.dumps(row, sort_keys=True) + "\n")
        stream.flush()
        os.fsync(stream.fileno())


def run(root: Path, candidate: Bot, opponents: list[Bot], maps: list[str], seeds: list[int], repeats: int = 1, jobs: int = 1, sandbox: bool = True, timeout: float = 120, out: Path | None = None, runner: str = "unswbc", keep_output: bool = False, keep_replays: bool = False, dry_run: bool = False, resume: Path | None = None, verbose: bool = False) -> Path:
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    run_dir = resume or (out or root / "results" / "runs" / f"{timestamp}-{git_sha(root)}" )
    run_dir.mkdir(parents=True, exist_ok=True)
    jsonl = run_dir / "games.jsonl"
    existing = {json.loads(line)["index"] for line in jsonl.read_text(encoding="utf-8").splitlines() if line.strip()} if jsonl.exists() else set()
    items = schedule(opponents, maps, seeds, repeats)
    items = [{**item, "index": index + 1, "dry_run": dry_run} for index, item in enumerate(items) if index + 1 not in existing]
    lock = threading.Lock()
    with ThreadPoolExecutor(max_workers=max(1, jobs)) as pool:
        futures = [pool.submit(run_one, root, run_dir, candidate, item["opponent"], item, runner, timeout, sandbox, keep_output, keep_replays, root / "config" / "death_rules.toml", verbose) for item in items]
        for position, future in enumerate(as_completed(futures), 1):
            row = future.result()
            _append(jsonl, row, lock)
            print(f"[{len(existing) + position}/{len(existing) + len(items)}] {row['candidate']} vs {row['opponent']} | {row['status'].upper()} r={row.get('rounds') or '-'}")
    return run_dir
