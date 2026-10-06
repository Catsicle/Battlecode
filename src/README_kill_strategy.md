# Kill Strategy

## Function

```cpp
KillResult kill_queen(
    const unswbc::Vision& vision,
    unswbc::Position queen_head,
    int length
);
```

### Input

- `vision` — current 7×7 vision.
- `queen_head` — position of the enemy Queen's head.
- `length` — attacker's current length.

### Output

```cpp
enum class KillAction {
    NONE,
    KILL_QUEEN,
    KILL_ENEMY_HEAD,
    PREPARE_QUEEN
};

struct KillResult {
    KillAction action;
    std::string path;
    unswbc::Position target;
};
```

`path` is a sequence of directions such as:

```text
NEEES
```

## Core Logic

Priority:

```text
1. Kill Queen if possible this turn.
2. Otherwise kill another enemy HEAD.
3. Otherwise prepare for the Queen.
```

Movement rules:

```text
free_steps = ceil(length / 4)
```

After free steps, each move is paid.

Pearls:

```text
free + pearl  → length + 1
paid + no pearl → length - 1
paid + pearl → net length 0
```

A pearl does **not** make a paid move free. Payment is checked first.

Collision:

```text
own body      → blocked
Queen body    → blocked
enemy body    → blocked
enemy HEAD    → targetable only if it is the selected target
```

KELP is an edge between tiles and cannot be crossed.

Portal edges are not used by the local 7×7 planner.

The search updates the current length and consumed pearls after every step.
