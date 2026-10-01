from __future__ import annotations

import argparse
import os
import shutil
import sys
from pathlib import Path

from .elo import write as write_elo
from .gates import evaluate
from .registry import discover, select
from .report import rows, write as write_report
from .runner import run
from .snapshot import create


def _root() -> Path:
    return Path(__file__).resolve().parents[2]


def _maps(root: Path, value: str) -> list[str]:
    if value in {"train", "all"}:
        result = [str(path.relative_to(root)) for path in sorted((root / "maps" / "train").glob("*.map"))]
    else:
        result = []
        for item in value.split(","):
            paths = sorted(root.glob(item))
            result.extend(str(path.relative_to(root)) for path in paths if path.is_file())
    if value == "holdout":
        result = [str(path.relative_to(root)) for path in sorted((root / "maps" / "holdout").glob("*.map"))]
    return result


def _run(args: argparse.Namespace, tier: str | None = None) -> int:
    root = _root()
    bots = discover(root)
    candidate = next((bot for bot in bots if bot.id == args.candidate), None)
    if candidate is None:
        print(f"unknown candidate: {args.candidate}", file=sys.stderr)
        return 2
    opponents = [bot for bot in select(bots, args.opponents or "baselines,opponents") if bot.id != candidate.id]
    maps = _maps(root, args.maps)
    if not maps:
        print("no maps matched", file=sys.stderr)
        return 2
    seeds = args.seeds
    if args.games:
        total_per_repeat = max(1, len(opponents) * len(maps) * len(seeds) * 2)
        args.repeats = max(1, (args.games + total_per_repeat - 1) // total_per_repeat)
    run_dir = run(root, candidate, opponents, maps, seeds, args.repeats, args.jobs, args.sandbox, args.timeout, Path(args.out).resolve() if args.out else None, args.runner, args.keep_output, args.keep_replays, args.dry_run, Path(args.resume) if args.resume else None, args.verbose)
    write_report(run_dir)
    write_elo(root)
    print(f"Report: {run_dir / 'report.md'}")
    return 1 if any(item["status"] == "error" for item in rows(run_dir)) else 0


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="bctool")
    sub = p.add_subparsers(dest="command", required=True)
    list_parser = sub.add_parser("list")
    list_parser.set_defaults(func=lambda args: (print("\n".join(bot.id for bot in discover(_root()))) or 0))
    run_parser = sub.add_parser("run")
    run_parser.add_argument("--candidate", default="active")
    run_parser.add_argument("--opponents", default="baselines,opponents")
    run_parser.add_argument("--maps", default="train")
    run_parser.add_argument("--seeds", nargs="+", type=int, default=[0])
    run_parser.add_argument("--games-per-matchup", dest="repeats", type=int, default=1)
    run_parser.add_argument("--games", type=int)
    run_parser.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 2) - 1))
    run_parser.add_argument("--sandbox", action=argparse.BooleanOptionalAction, default=True)
    run_parser.add_argument("--timeout", type=float, default=120)
    run_parser.add_argument("--runner", default="unswbc")
    run_parser.add_argument("--out")
    run_parser.add_argument("--resume")
    run_parser.add_argument("--keep-output", action="store_true")
    run_parser.add_argument("--keep-replays", action="store_true")
    run_parser.add_argument("--dry-run", action="store_true")
    run_parser.add_argument("--verbose", action="store_true", help="stream unswbc round output while games run")
    run_parser.set_defaults(func=_run)
    for name, maps, games, opponents, seeds in (
        ("smoke", "maps/train/arena.map", 2, "Random", [0]),
        ("quick", "train", 40, "baselines,opponents", [0, 1, 2, 3, 4]),
        ("full", "train", 400, "baselines,opponents", [0, 1, 2, 3, 4]),
    ):
        tier_parser = sub.add_parser(name)
        tier_parser.add_argument("--games", type=int, default=games)
        tier_parser.add_argument("--candidate", default="active")
        tier_parser.add_argument("--opponents", default=opponents)
        tier_parser.add_argument("--maps", default=maps)
        tier_parser.add_argument("--seeds", nargs="+", type=int, default=seeds)
        tier_parser.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 2) - 1))
        tier_parser.add_argument("--sandbox", action=argparse.BooleanOptionalAction, default=True)
        tier_parser.add_argument("--timeout", type=float, default=120)
        tier_parser.add_argument("--runner", default="unswbc")
        tier_parser.add_argument("--out")
        tier_parser.add_argument("--keep-output", action="store_true")
        tier_parser.add_argument("--keep-replays", action="store_true")
        tier_parser.add_argument("--dry-run", action="store_true")
        tier_parser.add_argument("--verbose", action="store_true", help="stream unswbc round output while games run")
        tier_parser.set_defaults(func=_run, repeats=1, resume=None)
    report_parser = sub.add_parser("report")
    report_parser.add_argument("run", type=Path)
    report_parser.set_defaults(func=lambda args: (print(write_report(args.run)) or 0))
    elo_parser = sub.add_parser("elo")
    elo_parser.set_defaults(func=lambda args: (print(write_elo(_root())) or 0))
    gate_parser = sub.add_parser("gates")
    gate_parser.add_argument("run", type=Path)
    gate_parser.set_defaults(func=lambda args: (print("\n".join(f"{r['status']}: {r['gate']} ({r['detail']})" for r in evaluate(args.run))) or 0))
    snap = sub.add_parser("snapshot")
    snap.add_argument("name")
    snap.add_argument("--note", default="")
    snap.add_argument("--force", action="store_true")
    snap.set_defaults(func=lambda args: (print(create(_root(), args.name, args.note, args.force)) or 0))
    replay = sub.add_parser("replay-cmd")
    replay.add_argument("run", type=Path)
    replay.add_argument("index", type=int)
    replay.set_defaults(func=lambda args: _replay_command(args))
    return p


def _replay_command(args: argparse.Namespace) -> int:
    import json
    rows = [json.loads(line) for line in (args.run / "games.jsonl").read_text(encoding="utf-8").splitlines() if line.strip()]
    row = next(row for row in rows if row["index"] == args.index)
    print(" ".join(row["command"]))
    return 0


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    return int(args.func(args))
