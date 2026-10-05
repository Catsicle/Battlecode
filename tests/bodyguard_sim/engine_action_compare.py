from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from bctool.replay import load


def compare_replay(path: Path) -> tuple[int, list[tuple[int, int, object, object]]]:
    replay = load(path)
    if not replay.get("available"):
        raise RuntimeError(replay.get("reason", "replay unavailable"))
    round_number = 0
    checked = 0
    mismatches: list[tuple[int, int, object, object]] = []
    for event in replay["data"]["events"]:
        if event["type"] == "roundStart":
            round_number = event["round"]
        if event["type"] != "dragonAction" or not event["action"]:
            continue
        steps = event["action"].get("steps", [])
        expected = "NESW"[(round_number * 3 + event["id"] * 5) % 4]
        checked += 1
        if event["action"].get("kind") != "move" or len(steps) != 1 or steps[0] != expected:
            mismatches.append((round_number, event["id"], steps, expected))
    return checked, mismatches


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare engine probe actions with TraceScript.")
    parser.add_argument("--games", type=int, default=1)
    args = parser.parse_args()
    total_checked = 0
    total_mismatches = 0
    with tempfile.TemporaryDirectory(prefix="bodyguard-engine-compare-") as directory:
        output_dir = Path(directory)
        for seed in range(args.games):
            replay_path = output_dir / f"{seed}.replay"
            command = [
                "uv",
                "run",
                "unswbc",
                "run",
                "maps/train/big_empty.map",
                "tests/bodyguard_sim/engine_probe",
                "tests/bodyguard_sim/engine_probe",
                "--seed",
                str(seed),
                "--no-logs",
                "--no-indicator",
                "--no-draw",
                "--output",
                str(replay_path),
            ]
            result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, check=False)
            if result.returncode != 0:
                print(f"seed {seed}: runner failed ({result.returncode})", file=sys.stderr)
                return 1
            checked, mismatches = compare_replay(replay_path)
            total_checked += checked
            total_mismatches += len(mismatches)
            if mismatches:
                print(f"seed {seed}: first mismatch {mismatches[0]}", file=sys.stderr)
                return 1
    print(f"engine action traces passed: games={args.games} actions={total_checked} mismatches={total_mismatches}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
