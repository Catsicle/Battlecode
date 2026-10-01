from __future__ import annotations


def detect_loop(positions: list[object], pearl_rounds: set[int] | None = None, min_repeats: int = 3) -> list[dict]:
    pearls = pearl_rounds or set()
    episodes: list[dict] = []
    index = 0
    while index < len(positions):
        found = None
        for period in range(2, min(64, (len(positions) - index) // min_repeats) + 1):
            repeats = 1
            while index + (repeats + 1) * period <= len(positions) and positions[index + repeats * period:index + (repeats + 1) * period] == positions[index:index + period]:
                repeats += 1
            if repeats >= min_repeats and not any(index + offset in pearls for offset in range(repeats * period)):
                found = {"start_round": index + 1, "end_round": index + repeats * period, "period": period, "repeats": repeats, "persisted_to_end_or_death": index + repeats * period == len(positions)}
                break
        if found:
            episodes.append(found)
            index = found["end_round"]
        else:
            index += 1
    return episodes
