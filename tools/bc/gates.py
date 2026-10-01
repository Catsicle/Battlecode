from __future__ import annotations

from pathlib import Path

from .report import rows


def evaluate(run_dir: Path, sandbox: bool = True) -> list[dict[str, str]]:
    data = rows(run_dir)
    results = [{"gate": "errors", "status": "PASS" if not any(row.get("status") == "error" for row in data) else "FAIL", "detail": str(sum(row.get("status") == "error" for row in data))}]
    cpu = [team.get("max_M") for row in data for team in row.get("points", {}).values() if isinstance(team, dict) and isinstance(team.get("max_M"), (int, float))]
    results.append({"gate": "cpu", "status": "PASS" if sandbox and cpu and max(cpu) <= 50 else ("FAIL" if not sandbox or not cpu else "FAIL"), "detail": "not measured" if not sandbox or not cpu else f"max {max(cpu):.2f}M"})
    return results
