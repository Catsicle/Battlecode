from __future__ import annotations

from pathlib import Path


def run(root: Path) -> int:
    scenario_dir = root / "scenarios"
    if not scenario_dir.exists():
        print("No scenarios configured.")
        return 0
    print("Scenario execution requires an installed unswbc runner.")
    return 2
