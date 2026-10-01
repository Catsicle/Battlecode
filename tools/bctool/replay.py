from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Any


class ReplayFormatError(ValueError):
    pass


def _unpack_packed(raw: bytes) -> bytes:
    output = bytearray()
    index = 0
    previous_tag = 0x77
    while index < len(raw):
        if previous_tag == 0:
            output.extend(b"\0" * (raw[index] * 8))
            index += 1
            previous_tag = 0x77
        elif previous_tag == 0xFF:
            count = raw[index] * 8
            index += 1
            if index + count > len(raw):
                raise ReplayFormatError("truncated packed literal run")
            output.extend(raw[index:index + count])
            index += count
            previous_tag = 0x77
        else:
            tag = raw[index]
            index += 1
            for bit in range(8):
                if tag & (1 << bit):
                    if index >= len(raw):
                        raise ReplayFormatError("truncated packed byte")
                    output.append(raw[index])
                    index += 1
                else:
                    output.append(0)
            previous_tag = tag
    return bytes(output)


def _signed_offset(value: int) -> int:
    value &= 0x3FFFFFFF
    return value - (1 << 30) if value & (1 << 29) else value


@dataclass(frozen=True)
class _Struct:
    message: "_Message"
    segment: int
    data_index: int
    data_words: int
    pointer_index: int
    pointer_words: int

    def _data_word(self, byte_offset: int) -> int:
        if byte_offset < 0 or byte_offset // 8 >= self.data_words:
            return 0
        return self.message.words[self.segment][self.data_index + byte_offset // 8]

    def u16(self, byte_offset: int) -> int:
        return (self._data_word(byte_offset) >> ((byte_offset % 8) * 8)) & 0xFFFF

    def u32(self, byte_offset: int) -> int:
        return (self._data_word(byte_offset) >> ((byte_offset % 8) * 8)) & 0xFFFFFFFF

    def u64(self, byte_offset: int) -> int:
        return self._data_word(byte_offset)

    def i32(self, byte_offset: int) -> int:
        return struct.unpack("<i", struct.pack("<I", self.u32(byte_offset)))[0]

    def bit(self, bit_offset: int) -> bool:
        return bool((self._data_word((bit_offset // 64) * 8) >> (bit_offset % 64)) & 1)

    def pointer(self, ordinal: int) -> tuple[int, int] | None:
        if ordinal >= self.pointer_words:
            return None
        return self.segment, self.pointer_index + ordinal

    def child(self, ordinal: int) -> "_Struct | None":
        pointer = self.pointer(ordinal)
        return self.message.struct_from_pointer(pointer) if pointer else None

    def text(self, ordinal: int) -> str:
        pointer = self.pointer(ordinal)
        return self.message.text_from_pointer(pointer) if pointer else ""

    def structs(self, ordinal: int) -> list["_Struct"]:
        pointer = self.pointer(ordinal)
        return self.message.struct_list(pointer) if pointer else []

    def bytes_list(self, ordinal: int) -> bytes:
        pointer = self.pointer(ordinal)
        return self.message.byte_list(pointer) if pointer else b""

    def u16_list(self, ordinal: int) -> list[int]:
        pointer = self.pointer(ordinal)
        return self.message.u16_list(pointer) if pointer else []


class _Message:
    def __init__(self, raw: bytes) -> None:
        data = _unpack_packed(raw)
        if len(data) < 8 or len(data) % 8:
            raise ReplayFormatError("invalid Cap'n Proto message length")
        segment_count = struct.unpack_from("<I", data, 0)[0] + 1
        if segment_count < 1 or len(data) < (1 + segment_count) * 4:
            raise ReplayFormatError("invalid Cap'n Proto segment table")
        sizes = [struct.unpack_from("<I", data, 4 + index * 4)[0] for index in range(segment_count)]
        header_words = (1 + segment_count + 1) // 2
        offset = header_words * 8
        self.words: list[list[int]] = []
        for size in sizes:
            end = offset + size * 8
            if end > len(data):
                raise ReplayFormatError("truncated Cap'n Proto segment")
            self.words.append(list(struct.unpack_from(f"<{size}Q", data, offset)))
            offset = end

    def _pointer_word(self, pointer: tuple[int, int]) -> int:
        segment, index = pointer
        return self.words[segment][index]

    def _resolve_pointer(self, pointer: tuple[int, int]) -> tuple[tuple[int, int], int]:
        word = self._pointer_word(pointer)
        if word & 3 != 2:
            return pointer, word
        if word & 4:
            raise ReplayFormatError("double-far pointers are not supported in replay messages")
        segment = word >> 32
        index = (word >> 3) & 0x1FFFFFFF
        return self._resolve_pointer((segment, index))

    def _target(self, pointer: tuple[int, int], word: int) -> tuple[int, int]:
        segment, index = pointer
        return segment, index + 1 + _signed_offset(word >> 2)

    def struct_from_pointer(self, pointer: tuple[int, int] | None) -> _Struct | None:
        if pointer is None:
            return None
        pointer, word = self._resolve_pointer(pointer)
        if word == 0 or word & 3 != 0:
            return None
        segment, index = self._target(pointer, word)
        data_words = (word >> 32) & 0xFFFF
        return _Struct(self, segment, index, data_words, index + data_words, (word >> 48) & 0xFFFF)

    def _list_info(self, pointer: tuple[int, int] | None) -> tuple[int, int, int, int] | None:
        if pointer is None:
            return None
        pointer, word = self._resolve_pointer(pointer)
        if word == 0 or word & 3 != 1:
            return None
        segment, index = self._target(pointer, word)
        return segment, index, (word >> 32) & 7, (word >> 35) & 0x1FFFFFFF

    def byte_list(self, pointer: tuple[int, int] | None) -> bytes:
        info = self._list_info(pointer)
        if info is None:
            return b""
        segment, index, element_size, length = info
        if element_size != 2:
            raise ReplayFormatError("expected a byte list")
        raw = b"".join(struct.pack("<Q", word) for word in self.words[segment][index:index + (length + 7) // 8])
        return raw[:length]

    def text_from_pointer(self, pointer: tuple[int, int] | None) -> str:
        return self.byte_list(pointer).rstrip(b"\0").decode("utf-8", errors="replace")

    def u16_list(self, pointer: tuple[int, int] | None) -> list[int]:
        info = self._list_info(pointer)
        if info is None:
            return []
        segment, index, element_size, length = info
        if element_size != 3:
            raise ReplayFormatError("expected a uint16 list")
        return [(self.words[segment][index + offset // 4] >> ((offset % 4) * 16)) & 0xFFFF for offset in range(length)]

    def struct_list(self, pointer: tuple[int, int] | None) -> list[_Struct]:
        info = self._list_info(pointer)
        if info is None:
            return []
        segment, index, element_size, word_length = info
        if element_size != 7:
            raise ReplayFormatError("expected a composite struct list")
        tag = self.words[segment][index]
        count = (tag >> 2) & 0x3FFFFFFF
        data_words = (tag >> 32) & 0xFFFF
        pointer_words = (tag >> 48) & 0xFFFF
        stride = data_words + pointer_words
        start = index + 1
        if start + count * stride > index + 1 + word_length:
            raise ReplayFormatError("invalid composite list length")
        return [_Struct(self, segment, start + n * stride, data_words, start + n * stride + data_words, pointer_words) for n in range(count)]

    def root(self) -> _Struct:
        root = self.struct_from_pointer((0, 0))
        if root is None:
            raise ReplayFormatError("replay root is not a struct")
        return root


def _point(value: _Struct | None) -> dict[str, int] | None:
    return None if value is None else {"x": value.i32(0), "y": value.i32(4)}


def _action(value: _Struct | None) -> dict[str, Any] | None:
    if value is None:
        return None
    kind = value.u16(0)
    if kind == 0:
        return {"kind": "move", "steps": ["NESW"[ordinal] if ordinal < 4 else str(ordinal) for ordinal in value.u16_list(0)]}
    if kind == 1:
        return {"kind": "split", "child_segment_count": value.i32(4)}
    if kind == 2:
        return {"kind": "suicide"}
    return {"kind": "unknown", "ordinal": kind}


def _event(value: _Struct) -> dict[str, Any]:
    kind = value.u16(0)
    variant = value.child(0)
    if kind == 0:
        return {"type": "roundStart", "round": variant.i32(0) if variant else 0}
    if kind == 1:
        return {"type": "turnStart", "id": variant.i32(0) if variant else -1}
    if kind == 2:
        return {"type": "pearlCountdown", "tile": _point(variant.child(0) if variant else None), "countdown": variant.i32(0) if variant else -1}
    if kind == 3:
        return {"type": "tileChange", "tile": _point(variant.child(0) if variant else None), "hasPearl": variant.bit(0) if variant else False}
    if kind == 4:
        instructions = variant.child(1) if variant else None
        return {"type": "dragonAction", "id": variant.i32(0) if variant else -1, "action": _action(variant.child(0) if variant else None), "instructions": instructions.u32(0) if instructions else None, "instruction_exceeded": instructions.bit(64) if instructions else False}
    if kind in (5, 6, 7):
        return {"type": {5: "engineLog", 6: "dragonLog", 7: "dragonIndicator"}[kind], "id": variant.i32(0) if variant else -1, "text": variant.text(0) if variant else ""}
    if kind == 9:
        return {"type": "dragonUpdate", "id": variant.i32(0) if variant else -1, "facing": variant.u16(4) if variant else 0, "head": _point(variant.child(0) if variant else None), "tail": _point(variant.child(1) if variant else None)}
    if kind == 10:
        return {"type": "dragonSplit", "parentId": variant.i32(0) if variant else -1, "childId": variant.i32(4) if variant else -1, "team": "B" if variant and variant.u16(8) else "A", "parentBody": [_point(item) for item in variant.structs(0)] if variant else [], "childBody": [_point(item) for item in variant.structs(1)] if variant else []}
    if kind == 11:
        reasons = ["hit a wall", "hit itself", "hit another dragon", "lost a head-to-head", "no valid action"]
        reason = variant.u16(4) if variant else 0
        return {"type": "dragonDeath", "id": variant.i32(0) if variant else -1, "reason": reasons[reason] if reason < len(reasons) else f"unknown reason {reason}"}
    return {"type": "ignored", "ordinal": kind}


def _parse(raw: bytes) -> dict[str, Any]:
    root = _Message(raw).root()
    format_version = root.u32(0)
    if format_version > 2:
        raise ReplayFormatError(f"replay format {format_version} requires a newer parser")
    result = root.child(4)
    teams = {}
    for name, value in (("A", result.child(0) if result else None), ("B", result.child(1) if result else None)):
        teams[name] = {"dragon_count": value.i32(0) if value else None, "longest_dragon": value.i32(4) if value else None, "total_length": value.i32(8) if value else None}
    winner = None
    if result and result.u16(4) == 1:
        winner = "B" if result.u16(6) else "A"
    seed = root.u64(8) if root.u16(4) == 1 else None
    return {"format_version": format_version, "map": root.text(0), "bot_a": root.text(1), "bot_b": root.text(2), "seed": seed, "events": [_event(item) for item in root.structs(3)], "result": {"terminated": bool(result.bit(0)) if result else False, "end_reason": "round_limit" if result and result.u16(2) == 1 else "team_eliminated", "winner": winner, "teams": teams}}


def load(path: Path | None) -> dict[str, Any]:
    if path is None:
        return {"available": False, "reason": "no replay was retained"}
    try:
        value = _parse(path.read_bytes())
    except (OSError, ReplayFormatError, struct.error) as exc:
        return {"available": False, "reason": f"replay could not be decoded: {exc}"}
    return {"available": True, "data": value}


def unavailable(reason: str) -> dict[str, Any]:
    return {"value": None, "reason": reason}