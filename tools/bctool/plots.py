from __future__ import annotations

from pathlib import Path


def render(run_dir: Path, rows: list[dict]) -> list[Path]:
    """Render optional plots; absence of matplotlib is a valid installation state."""
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        return []
    output = run_dir / "plots"
    output.mkdir(exist_ok=True)
    statuses = {}
    for row in rows:
        statuses[row.get("status", "error")] = statuses.get(row.get("status", "error"), 0) + 1
    figure = plt.figure(figsize=(6, 4))
    plt.bar(list(statuses), list(statuses.values()))
    plt.title("Run outcomes")
    path = output / "outcomes.png"
    figure.savefig(path, dpi=120, bbox_inches="tight")
    plt.close(figure)
    return [path]
