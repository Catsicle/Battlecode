from __future__ import annotations

from collections import Counter, defaultdict
from typing import Any

from .deaths import classify
from .loops import detect_loop
from .replay import unavailable


REPLAY_METRICS = ("longest_dragon_end", "dragons_end", "pearls_collected", "pearls_total", "no_legal_move_turns", "stuck_loop_turns", "loops_count", "loops", "loop_len_mean", "loop_len_max")


def unavailable_metrics(reason: str = "replay schema is not verified") -> dict[str, Any]:
    return {name: unavailable(reason) for name in REPLAY_METRICS}


def from_game(points: dict[str, Any], replay: dict[str, Any]) -> dict[str, Any]:
    result = unavailable_metrics()
    result["cpu_p50_M"] = {team: value.get("p50_M") for team, value in points.items() if isinstance(value, dict)}
    result["cpu_p99_M"] = {team: value.get("p99_M") for team, value in points.items() if isinstance(value, dict)}
    result["replay_available"] = bool(replay.get("available"))
    return result


def _map_info(text: str) -> tuple[int, int, dict[int, tuple[int, int]], dict[int, list[tuple[int, int]]]]:
    width = height = 0
    edges: dict[int, tuple[int, int]] = {}
    dragons: dict[int, list[tuple[int, int]]] = {}
    for line in text.splitlines():
        fields = line.split()
        if not fields:
            continue
        if fields[0] == "MAP":
            width, height = int(fields[1]), int(fields[2])
        elif fields[0] == "EDGE":
            edges[int(fields[1])] = (int(fields[2]), int(fields[3]))
        elif fields[0] == "DRAGON":
            team, length = int(fields[1]), int(fields[2])
            values = [int(value) for value in fields[3:3 + length * 2]]
            dragons[len(dragons)] = ("B" if team else "A", [(values[index], values[index + 1]) for index in range(0, len(values), 2)])
    return width, height, edges, dragons


def _edge_index(width: int, x: int, y: int, direction: str) -> int:
    if direction == "N":
        return (2 * y) * (width + 1) + x
    if direction == "S":
        return (2 * (y + 1)) * (width + 1) + x
    if direction == "W":
        return (2 * y + 1) * (width + 1) + x
    return (2 * y + 1) * (width + 1) + x + 1


def _step(position: tuple[int, int], direction: str, width: int, height: int, edges: dict[int, tuple[int, int]]) -> tuple[int, int] | None:
    x, y = position
    edge_kind, edge_id = edges.get(_edge_index(width, x, y, direction), (0, -1))
    if edge_kind == 1:
        return None
    if edge_kind == 2:
        paired = next((index for index, value in edges.items() if value == (2, edge_id) and index != _edge_index(width, x, y, direction)), None)
        if paired is None:
            return None
        row, column = divmod(paired, width + 1)
        if row % 2 == 0:
            return (column % width, (row // 2 - (1 if direction == "N" else 0)) % height)
        return ((column - (1 if direction == "W" else 0)) % width, ((row - 1) // 2) % height)
    deltas = {"N": (0, -1), "E": (1, 0), "S": (0, 1), "W": (-1, 0)}
    dx, dy = deltas[direction]
    return ((x + dx) % width, (y + dy) % height)


def _has_legal_move(state: dict[int, dict[str, Any]], dragon_id: int, width: int, height: int, edges: dict[int, tuple[int, int]]) -> bool:
    body = state.get(dragon_id, {}).get("body", [])
    if not body:
        return False
    occupied = {tile for dragon in state.values() if dragon.get("alive", True) for tile in dragon.get("body", [])}
    return any((destination := _step(body[0], direction, width, height, edges)) is not None and destination not in occupied for direction in "NESW")


def analyze_replay(data: dict[str, Any], rules: dict[str, Any] | None = None) -> dict[str, Any]:
    width, height, edges, initial_dragons = _map_info(data.get("map", ""))
    state: dict[int, dict[str, Any]] = {}
    for dragon_id, (team, body) in initial_dragons.items():
        state[dragon_id] = {"team": team, "body": body, "alive": True}
    # The map's DRAGON order is interleaved by team in the engine, so the first
    # two teams are recovered from the initial update/death stream when needed.
    team_by_id: dict[int, str] = {dragon_id: value["team"] for dragon_id, value in state.items()}
    heads: dict[int, list[tuple[int, int]]] = defaultdict(list)
    head_rounds: dict[int, list[int]] = defaultdict(list)
    pearl_state: dict[tuple[int, int], bool] = {}
    pearl_total = 0
    pearls_collected = Counter({"A": 0, "B": 0})
    pearl_rounds: set[int] = set()
    deaths: Counter[str] = Counter()
    death_causes: Counter[str] = Counter()
    no_legal = Counter({"A": 0, "B": 0})
    current_round = 0
    current_turn: int | None = None
    curves: dict[str, dict[str, list[int]]] = {team: {"longest_len": [], "total_len": [], "unit_count": []} for team in "AB"}

    for event in data.get("events", []):
        event_type = event.get("type")
        if event_type == "roundStart":
            current_round = int(event["round"])
            for team in "AB":
                living = [value["body"] for value in state.values() if value.get("alive") and value.get("team") == team]
                curves[team]["longest_len"].append(max((len(body) for body in living), default=0))
                curves[team]["total_len"].append(sum(len(body) for body in living))
                curves[team]["unit_count"].append(len(living))
        elif event_type == "turnStart":
            current_turn = int(event["id"])
        elif event_type == "dragonAction":
            if event.get("action", {}).get("kind") == "suicide" and current_turn is not None and not _has_legal_move(state, current_turn, width, height, edges):
                no_legal[team_by_id.get(current_turn, "A")] += 1
        elif event_type == "tileChange":
            tile = event.get("tile")
            if tile is None:
                continue
            key = (int(tile["x"]), int(tile["y"]))
            present = bool(event.get("hasPearl"))
            previous = pearl_state.get(key, False)
            if present and not previous:
                pearl_total += 1
            elif previous and not present:
                if current_turn is not None:
                    pearls_collected[team_by_id.get(current_turn, "A")] += 1
                pearl_rounds.add(current_round)
            pearl_state[key] = present
        elif event_type == "dragonUpdate":
            dragon_id = int(event["id"])
            head = event.get("head")
            tail = event.get("tail")
            if head is None:
                continue
            head_position = (int(head["x"]), int(head["y"]))
            heads[dragon_id].append(head_position)
            head_rounds[dragon_id].append(current_round)
            if current_round == 0 and not heads[dragon_id][:-1]:
                if dragon_id not in state:
                    state[dragon_id] = {"team": team_by_id.get(dragon_id, "A"), "body": [head_position], "alive": True}
                continue
            dragon = state.setdefault(dragon_id, {"team": team_by_id.get(dragon_id, "A"), "body": [], "alive": True})
            old_body = list(dragon.get("body", []))
            updated = [head_position] + old_body
            tail_position = (int(tail["x"]), int(tail["y"])) if tail else None
            while len(updated) > 1 and tail_position is not None and updated[-1] != tail_position:
                updated.pop()
            dragon["body"] = updated
            dragon["alive"] = True
        elif event_type == "dragonSplit":
            parent_id = int(event["parentId"])
            child_id = int(event["childId"])
            team = str(event.get("team", "A"))
            team_by_id[parent_id] = team_by_id.get(parent_id, team)
            team_by_id[child_id] = team
            state[parent_id] = {"team": team_by_id[parent_id], "body": [(int(p["x"]), int(p["y"])) for p in event.get("parentBody", [])], "alive": True}
            state[child_id] = {"team": team, "body": [(int(p["x"]), int(p["y"])) for p in event.get("childBody", [])], "alive": True}
        elif event_type == "dragonDeath":
            dragon_id = int(event["id"])
            cause = str(event.get("reason", "unknown"))
            team = team_by_id.get(dragon_id, state.get(dragon_id, {}).get("team", "A"))
            category = classify(cause, rules)
            deaths[team] += 1
            death_causes[cause] += 1
            state.setdefault(dragon_id, {"team": team, "body": [], "alive": True})["alive"] = False

    loops_by_team: dict[str, list[dict[str, Any]]] = {"A": [], "B": []}
    loop_turns = Counter({"A": 0, "B": 0})
    for dragon_id, positions in heads.items():
        pearl_indices = {index for index, round_number in enumerate(head_rounds[dragon_id]) if round_number in pearl_rounds}
        for episode in detect_loop(positions, pearl_indices):
            start_index = max(0, int(episode["start_round"]) - 1)
            end_index = min(len(head_rounds[dragon_id]) - 1, int(episode["end_round"]) - 1)
            team = team_by_id.get(dragon_id, "A")
            loops_by_team[team].append({"dragon_id": dragon_id, "start_round": head_rounds[dragon_id][start_index], "end_round": head_rounds[dragon_id][end_index], "period": episode["period"], "repeats": episode["repeats"], "persisted_to_end_or_death": episode["persisted_to_end_or_death"]})
            loop_turns[team] += max(1, end_index - start_index + 1)

    result_teams = data.get("result", {}).get("teams", {})
    output: dict[str, Any] = {"teams": {}, "curves": curves, "unclassified_death_causes": sorted(cause for cause in death_causes if classify(cause, rules) == "unclassified")}
    for team in "AB":
        loops = loops_by_team[team]
        periods = [loop["period"] for loop in loops]
        team_death_categories = Counter()
        for event in data.get("events", []):
            if event.get("type") == "dragonDeath" and team_by_id.get(int(event["id"]), "A") == team:
                team_death_categories[classify(str(event.get("reason", "unknown")), rules)] += 1
        standing = result_teams.get(team, {})
        output["teams"][team] = {"longest_dragon_end": standing.get("longest_dragon"), "dragons_end": standing.get("dragon_count"), "pearls_collected": pearls_collected[team], "pearls_total": pearl_total, "pearls_total_reason": "count of pearl appearance events in replay, including drops", "deaths_total": deaths[team], "deaths_suicide": team_death_categories["suicide"], "deaths_attacked": team_death_categories["deliberate_attack"], "deaths_other": team_death_categories["other"], "deaths_unclassified": team_death_categories["unclassified"], "no_legal_move_turns": no_legal[team], "stuck_loop_turns": loop_turns[team], "loops_count": len(loops), "loops": loops, "loop_len_mean": sum(periods) / len(periods) if periods else 0.0, "loop_len_max": max(periods, default=0)}
    return output
