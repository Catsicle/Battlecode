#!/usr/bin/env python3
"""Run repeatable Battlecode bot tournaments and summarize the results."""

from __future__ import annotations

import argparse
import csv
import json
import re
import shutil
import subprocess
import sys
from collections import defaultdict
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


WIN_RE = re.compile(r"team ([AB]) wins after (\d+) rounds(?: \(([^)]+)\))?")
DRAW_RE = re.compile(r"draw after (\d+) rounds(?: \(([^)]+)\))?")
POINTS_RE = re.compile(
    r"team ([AB]) points per turn:\s+p50\s+([\d.]+M)\s+p99\s+([\d.]+M)\s+"
    r"mean\s+([\d.]+M)\s+max\s+([\d.]+M)\s+\((\d+) turns\)"
)
DEATH_RE = re.compile(r"round (\d+): bot (\d+) \(team ([AB])\) died: (.+?)\s*$")


@dataclass
class GameResult:
    candidate: str
    opponent: str
    map: str
    seed: int
    candidate_team: str
    command: list[str]
    status: str
    winner: str | None = None
    rounds: int | None = None
    reason: str | None = None
    returncode: int = 0
    duration_seconds: float = 0.0
    points: dict[str, dict[str, Any]] = field(default_factory=dict)
    deaths: list[dict[str, Any]] = field(default_factory=list)
    stdout: str = ""
    stderr: str = ""
    replay: str | None = None


def discover_bots(root: Path) -> list[str]:
    bots = []
    for child in sorted(root.iterdir()):
        if not child.is_dir() or not (child / "bot.toml").is_file():
            continue
        if not any((child / name).is_file() for name in ("main.cpp", "main.c", "main.py", "main.cc")):
            continue
        bots.append(child.name)
    return bots


def parse_points(text: str) -> dict[str, dict[str, Any]]:
    points: dict[str, dict[str, Any]] = {}
    for match in POINTS_RE.finditer(text):
        team, p50, p99, mean, maximum, turns = match.groups()
        points[team] = {
            "p50": p50,
            "p99": p99,
            "mean": mean,
            "max": maximum,
            "turns": int(turns),
        }
    return points


def parse_game_output(text: str) -> tuple[str, str | None, int | None, str | None]:
    winner_match = WIN_RE.search(text)
    if winner_match:
        winner, rounds, reason = winner_match.groups()
        return "win", winner, int(rounds), reason
    draw_match = DRAW_RE.search(text)
    if draw_match:
        rounds, reason = draw_match.groups()
        return "draw", None, int(rounds), reason
    return "error", None, None, "runner produced no game result"


def parse_deaths(text: str) -> list[dict[str, Any]]:
    deaths = []
    for match in DEATH_RE.finditer(text):
        round_number, bot_id, team, cause = match.groups()
        deaths.append({
            "round": int(round_number),
            "bot_id": int(bot_id),
            "team": team,
            "cause": cause.strip(),
        })
    return deaths


def make_command(args: argparse.Namespace, bot_a: str, bot_b: str, map_path: str, seed: int, replay: Path | None) -> list[str]:
    command = [args.runner, "run", map_path, bot_a, bot_b, "--seed", str(seed)]
    if args.sandbox:
        command.append("--sandbox")
    if args.no_logs:
        command.append("--no-logs")
    if args.no_indicator:
        command.append("--no-indicator")
    if args.no_draw:
        command.append("--no-draw")
    if replay is None:
        command.append("--no-replay")
    else:
        command.extend(["--output", str(replay)])
    return command


def run_game(args: argparse.Namespace, candidate: str, opponent: str, map_path: str, seed: int,
             candidate_team: str, index: int, output_dir: Path) -> GameResult:
    if candidate_team == "A":
        bot_a, bot_b = candidate, opponent
    else:
        bot_a, bot_b = opponent, candidate

    replay = None
    if args.keep_replays:
        replay_dir = output_dir / "replays"
        replay_dir.mkdir(parents=True, exist_ok=True)
        replay = replay_dir / f"{index:04d}-{candidate}-vs-{opponent}-seed-{seed}.replay"

    command = make_command(args, bot_a, bot_b, map_path, seed, replay)
    if args.dry_run:
        print("DRY-RUN:", " ".join(command))
        return GameResult(candidate, opponent, map_path, seed, candidate_team, command, "dry-run")

    started = datetime.now(timezone.utc)
    try:
        completed = subprocess.run(
            command,
            cwd=args.root,
            capture_output=True,
            text=True,
            timeout=args.timeout,
            check=False,
        )
        stdout = completed.stdout
        stderr = completed.stderr
        returncode = completed.returncode
    except subprocess.TimeoutExpired as exc:
        stdout = exc.stdout or ""
        stderr = exc.stderr or ""
        return GameResult(
            candidate, opponent, map_path, seed, candidate_team, command, "error",
            reason=f"timeout after {args.timeout} seconds", returncode=-1,
            duration_seconds=(datetime.now(timezone.utc) - started).total_seconds(),
            stdout=stdout, stderr=stderr,
        )

    combined = f"{stdout}\n{stderr}"
    status, winner, rounds, reason = parse_game_output(combined)
    if returncode != 0 and status != "win" and status != "draw":
        status = "error"
        reason = reason or f"runner exited with code {returncode}"
    if status == "win":
        status = "win" if winner == candidate_team else "loss"
    return GameResult(
        candidate=candidate,
        opponent=opponent,
        map=map_path,
        seed=seed,
        candidate_team=candidate_team,
        command=command,
        status=status,
        winner=winner,
        rounds=rounds,
        reason=reason,
        returncode=returncode,
        duration_seconds=(datetime.now(timezone.utc) - started).total_seconds(),
        points=parse_points(combined),
        deaths=parse_deaths(combined),
        stdout=stdout if args.keep_output else "",
        stderr=stderr if args.keep_output else "",
        replay=str(replay) if replay and replay.exists() else None,
    )


def summarize(results: list[GameResult], candidate: str, bots: list[str], maps: list[str], seeds: list[int]) -> dict[str, Any]:
    counts = {key: 0 for key in ("games", "wins", "losses", "draws", "errors", "dry_runs")}
    by_opponent: dict[str, dict[str, int]] = defaultdict(lambda: {"games": 0, "wins": 0, "losses": 0, "draws": 0, "errors": 0})
    by_side: dict[str, dict[str, int]] = defaultdict(lambda: {"games": 0, "wins": 0, "losses": 0, "draws": 0, "errors": 0})
    rounds = []
    death_causes: dict[str, int] = defaultdict(int)

    for result in results:
        counts["games"] += 1
        count_key = {"win": "wins", "loss": "losses", "draw": "draws", "error": "errors", "dry-run": "dry_runs"}.get(result.status)
        if count_key is not None:
            counts[count_key] += 1
        if result.rounds is not None:
            rounds.append(result.rounds)
        opponent_stats = by_opponent[result.opponent]
        side_stats = by_side[result.candidate_team]
        for stats in (opponent_stats, side_stats):
            stats["games"] += 1
            stat_key = {"win": "wins", "loss": "losses", "draw": "draws", "error": "errors"}.get(result.status)
            if stat_key is not None:
                stats[stat_key] += 1
        for death in result.deaths:
            death_causes[death["cause"]] += 1

    decisive_games = counts["wins"] + counts["losses"]
    summary = {
        "candidate": candidate,
        "bots_tested": bots,
        "maps": maps,
        "seeds": seeds,
        "counts": counts,
        "win_rate": counts["wins"] / counts["games"] if counts["games"] else 0.0,
        "decisive_win_rate": counts["wins"] / decisive_games if decisive_games else 0.0,
        "average_rounds": sum(rounds) / len(rounds) if rounds else None,
        "min_rounds": min(rounds) if rounds else None,
        "max_rounds": max(rounds) if rounds else None,
        "wins_against": sorted(name for name, stats in by_opponent.items() if stats["wins"]),
        "losses_against": sorted(name for name, stats in by_opponent.items() if stats["losses"]),
        "by_opponent": dict(sorted(by_opponent.items())),
        "by_side": dict(sorted(by_side.items())),
        "death_causes_seen": dict(sorted(death_causes.items(), key=lambda item: (-item[1], item[0]))),
    }
    return summary


def write_csv(path: Path, results: list[GameResult]) -> None:
    fields = ["candidate", "opponent", "map", "seed", "candidate_team", "status", "winner", "rounds", "reason", "returncode", "duration_seconds", "replay"]
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for result in results:
            row = asdict(result)
            writer.writerow({field: row[field] for field in fields})


def print_summary(summary: dict[str, Any], results: list[GameResult]) -> None:
    counts = summary["counts"]
    print(f"\nCandidate: {summary['candidate']}")
    print(f"Games: {counts['games']} | Wins: {counts['wins']} | Losses: {counts['losses']} | Draws: {counts['draws']} | Errors: {counts['errors']}")
    print(f"Win rate: {summary['win_rate']:.1%} overall | {summary['decisive_win_rate']:.1%} of decisive games")
    print(f"Rounds: average={summary['average_rounds']} min={summary['min_rounds']} max={summary['max_rounds']}")
    print("\nBy opponent:")
    for opponent, stats in summary["by_opponent"].items():
        print(f"  {opponent}: {stats['wins']}W-{stats['losses']}L-{stats['draws']}D ({stats['games']} games)")
    print("\nBy side:")
    for side, stats in summary["by_side"].items():
        print(f"  Team {side}: {stats['wins']}W-{stats['losses']}L-{stats['draws']}D ({stats['games']} games)")
    if summary["death_causes_seen"]:
        print("\nObserved death causes:")
        for cause, count in summary["death_causes_seen"].items():
            print(f"  {cause}: {count}")
    errors = [result for result in results if result.status == "error"]
    if errors:
        print("\nErrors:")
        for result in errors:
            print(f"  {result.opponent} on {result.map}, seed {result.seed}: {result.reason}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", help="Bot directory to evaluate, for example Pearl2")
    parser.add_argument("--opponents", nargs="+", help="Opponent directory names; default is every other discovered bot")
    parser.add_argument("--maps", nargs="+", default=["maps/arena.map"], help="Map paths relative to the workspace")
    parser.add_argument("--seeds", nargs="+", type=int, default=[0], help="Deterministic game seeds")
    parser.add_argument("--games-per-matchup", type=int, default=1, help="Repeat each map/seed matchup this many times")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent, help=argparse.SUPPRESS)
    parser.add_argument("--runner", default=shutil.which("unswbc") or "unswbc", help="Path to unswbc")
    parser.add_argument("--output-dir", type=Path, default=Path("tournament_results"))
    parser.add_argument("--keep-replays", action="store_true", help="Save one replay per game")
    parser.add_argument("--keep-output", action="store_true", help="Store raw runner stdout/stderr in JSON")
    parser.add_argument("--sandbox", action="store_true", help="Run matches with judge-style CPU accounting")
    parser.add_argument("--timeout", type=float, default=120.0, help="Maximum seconds per game")
    parser.add_argument("--dry-run", action="store_true", help="Print commands without running games")
    parser.add_argument("--list-bots", action="store_true", help="List discovered bots and exit")
    parser.add_argument("--no-logs", action="store_true", default=True, help=argparse.SUPPRESS)
    parser.add_argument("--no-indicator", action="store_true", default=True, help=argparse.SUPPRESS)
    parser.add_argument("--no-draw", action="store_true", default=True, help=argparse.SUPPRESS)
    args = parser.parse_args()
    args.root = args.root.resolve()
    args.output_dir = (args.root / args.output_dir).resolve()
    return args


def main() -> int:
    args = parse_args()
    bots = discover_bots(args.root)
    if args.list_bots:
        print("\n".join(bots))
        return 0
    if not args.candidate:
        print("--candidate is required unless --list-bots is used", file=sys.stderr)
        return 2
    if args.candidate not in bots:
        print(f"Unknown candidate {args.candidate!r}. Discovered bots: {', '.join(bots)}", file=sys.stderr)
        return 2
    opponents = args.opponents or [bot for bot in bots if bot != args.candidate]
    unknown = sorted(set(opponents) - set(bots))
    if unknown:
        print(f"Unknown opponent(s): {', '.join(unknown)}", file=sys.stderr)
        return 2
    if args.games_per_matchup < 1:
        print("--games-per-matchup must be at least 1", file=sys.stderr)
        return 2

    args.output_dir.mkdir(parents=True, exist_ok=True)
    results: list[GameResult] = []
    game_index = 1
    for opponent in opponents:
        for map_path in args.maps:
            for seed in args.seeds:
                for _ in range(args.games_per_matchup):
                    for candidate_team in ("A", "B"):
                        print(f"[{game_index}] {args.candidate} vs {opponent} on {map_path}, seed {seed}, team {candidate_team}")
                        result = run_game(args, args.candidate, opponent, map_path, seed, candidate_team, game_index, args.output_dir)
                        results.append(result)
                        if not args.dry_run:
                            print(f"    {result.status} in {result.rounds or '-'} rounds")
                        game_index += 1

    summary = summarize(results, args.candidate, opponents, args.maps, args.seeds)
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    json_path = args.output_dir / f"{args.candidate}-{timestamp}.json"
    csv_path = args.output_dir / f"{args.candidate}-{timestamp}.csv"
    payload = {"summary": summary, "games": [asdict(result) for result in results]}
    json_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    write_csv(csv_path, results)
    print_summary(summary, results)
    print(f"\nJSON: {json_path}")
    print(f"CSV:  {csv_path}")
    return 0 if not any(result.status == "error" for result in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())