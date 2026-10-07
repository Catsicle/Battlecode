# Sonar Communication

64-bit sonar encoding/decoding system used by the bot.

## Sonar IDs

| ID | Type |
|---|---|
| `0000` | Suicide |
| `0001` | Portal |
| `0010` | Idle |
| `1111` | Enemy Queen |

## Authentication

```cpp
uint8_t authenticate(uint64_t round, uint8_t sonar_id,
                     uint64_t k0, uint64_t k1);
```

Uses SipHash-2-4 on:

```text
(round << 8) | sonar_id
```

Returns the top 6 bits of the hash.

Messages are accepted for the **current round or previous round**.

## Encoders

### Enemy Queen

```cpp
uint64_t enemy_queen_sonar(uint8_t x, uint8_t y, uint64_t round);
```

```text
[ID 4][X 6][Y 6][Random 42][Auth 6]
```

Returns a `uint64_t`.

### Idle

```cpp
uint64_t idle_sonar(
    uint8_t hx, uint8_t hy,
    const vector<vector<bool>>& pearl_grid,
    uint64_t round
);
```

```text
[ID 4][HX 6][HY 6][Pearls 36][Random 6][Auth 6]
```

`pearl_grid` is a `6×6` boolean grid.

### Portal

```cpp
uint64_t portal_sonar(
    uint8_t sx, uint8_t sy, uint8_t sdir,
    uint8_t x1, uint8_t y1,
    uint8_t x2, uint8_t y2, uint8_t dir2,
    uint8_t x3, uint8_t y3, uint8_t dir3,
    uint64_t round
);
```

```text
[ID 4][Start 14][Point1 12][Point2 14][Point3 14][Auth 6]
```

Directions:

```text
0 = North
1 = East
2 = South
3 = West
```

### Suicide

```cpp
uint64_t suicide_sonar(uint64_t round);
```

```text
[ID 4][Secret 54][Auth 6]
```

The 54-bit secret must match `SUICIDE_KEY`.

## Decoder

```cpp
vector<uint64_t> decode_sonar(uint64_t msg, uint64_t round);
```

Checks authentication, identifies the sonar from bits `63–60`, then decodes the corresponding fields.

Returns an empty vector `{}` if invalid.

### Output formats

**Enemy Queen:**
```text
[ID, X, Y]
```

**Idle:**
```text
[ID, HX, HY, pearl1 ... pearl36]
```

Pearls are stored row-by-row from `(0,0)` to `(5,5)`.

**Portal:**
```text
[ID, SX, SY, SDir, X1, Y1, X2, Y2, Dir2, X3, Y3, Dir3]
```

**Suicide:**
```text
[ID, Secret]
```

## Constants

```cpp
constexpr uint64_t SECRET_K0 = 0xA3B1C2D3E4F56789ULL;
constexpr uint64_t SECRET_K1 = 0x1F2E3D4C5B6A7988ULL;
constexpr uint64_t SUICIDE_KEY = 0xA7C39E5B1D4F82ULL;
```
