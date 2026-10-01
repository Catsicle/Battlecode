from __future__ import annotations

import math
from statistics import mean
from typing import Iterable


def wilson(w: int, n: int, z: float = 1.96) -> tuple[float, float]:
    if n == 0:
        return 0.0, 1.0
    p = w / n
    d = 1 + z * z / n
    c = (p + z * z / (2 * n)) / d
    h = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / d
    return c - h, c + h


def elo_diff(p: float) -> float:
    if p <= 0 or p >= 1:
        return float("-inf") if p <= 0 else float("inf")
    return 400 * math.log10(p / (1 - p))


def numeric(values: Iterable[object]) -> list[float]:
    return [float(value) for value in values if isinstance(value, (int, float))]


def average(values: Iterable[object]) -> float | None:
    numbers = numeric(values)
    return mean(numbers) if numbers else None
