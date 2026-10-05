from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SIMULATOR = Path("/tmp/battlecode-cmake/bodyguard-sim-tests")
sys.path.insert(0, str(ROOT / "tools"))

from bctool.replay import load


def simulator_states(seed: int, initial_pearls: list[int]) -> dict[tuple[int, int], tuple[int, int, int, int]]:
    command = [str(SIMULATOR), "--state-trace", "--seed", str(seed)]
    for pearl in initial_pearls:
        command.extend(("--pearl", str(pearl)))
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise RuntimeError(result.stderr or "simulator state trace failed")
    return {
        tuple(map(int, line.split()[:2])): tuple(map(int, line.split()[2:]))
        for line in result.stdout.splitlines()
    }


def initial_pearls(replay: dict) -> list[int]:
    result: list[int] = []
    for event in replay["data"]["events"]:
        if event["type"] == "dragonAction":
            break
        if event["type"] == "tileChange" and event["hasPearl"]:
            tile = event["tile"]
            result.append(tile["x"] + 64 * tile["y"])
    return result


def compare_replay(path: Path, states: dict[tuple[int, int], tuple[int, int, int, int]]) -> int:
    replay = load(path)
    if not replay.get("available"):
        raise RuntimeError(replay.get("reason", "replay unavailable"))
    updates: dict[tuple[int, int], dict] = {}
    deaths: set[tuple[int, int]] = set()
    current_round: int | None = None
    for event in replay["data"]["events"]:
        if event["type"] == "roundStart":
            current_round = event["round"]
        # Compare only post-turn updates assigned by roundStart (filter initial round=None)
        if current_round is not None and event["type"] == "dragonUpdate":
            updates[(current_round, event["id"])] = event
        elif current_round is not None and event["type"] == "dragonDeath":
            deaths.add((current_round, event["id"]))
    checked = 0
    for key, expected in states.items():
        event = updates.get(key)
        actual_alive = key not in deaths and event is not None
        if actual_alive != bool(expected[0]):
            raise RuntimeError(f"alive mismatch at {key}: engine={actual_alive} simulator={expected}")
        if event is None:
            continue
        actual_head = event["head"]["x"] + 64 * event["head"]["y"]
        actual_tail = event["tail"]["x"] + 64 * event["tail"]["y"]
        if (actual_head, actual_tail) != (expected[1], expected[2]):
            raise RuntimeError(f"position mismatch at {key}: engine={(actual_head, actual_tail)} simulator={expected[1:3]}")
        if expected[3] != 6:
            raise RuntimeError(f"unexpected simulator length at {key}: {expected[3]}")
        checked += 1
    return checked


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare controlled simulator state with engine replays.")
    parser.add_argument("--games", type=int, default=1)
    args = parser.parse_args()
    checked = 0
    with tempfile.TemporaryDirectory(prefix="bodyguard-engine-state-") as directory:
        for seed in range(args.games):
            replay_path = Path(directory) / f"{seed}.replay"
            command = [
                "uv", "run", "unswbc", "run", "maps/train/big_empty.map",
                "tests/bodyguard_sim/engine_probe", "tests/bodyguard_sim/engine_probe",
                "--seed", str(seed), "--no-logs", "--no-indicator", "--no-draw",
                "--output", str(replay_path),
            ]
            result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, check=False)
            if result.returncode != 0:
                print(f"seed {seed}: runner failed ({result.returncode})", file=sys.stderr)
                return 1
            replay = load(replay_path)
            if not replay.get("available"):
                print(f"seed {seed}: replay unavailable", file=sys.stderr)
                return 1
            states = simulator_states(0, initial_pearls(replay))
            try:
                checked += compare_replay(replay_path, states)
            except RuntimeError as error:
                print(f"seed {seed}: {error}", file=sys.stderr)
                return 1
    print(f"engine state traces passed: games={args.games} snapshots={checked}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
