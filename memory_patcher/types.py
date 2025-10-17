"""Value encoding helpers and supported search types."""

from __future__ import annotations

import binascii
import math
import struct
from dataclasses import dataclass
from enum import Enum
from typing import Any


class ValueType(str, Enum):
    INT32 = "int32"
    UINT32 = "uint32"
    FLOAT = "float"
    ASCII = "ascii"
    UTF16 = "utf16"
    HEX = "hex"


@dataclass(frozen=True)
class EncodedValue:
    raw: bytes
    display: str
    type: ValueType

    @property
    def size(self) -> int:
        return len(self.raw)


def encode_value(value_type: ValueType, value: str) -> EncodedValue:
    value = value.strip()
    if value_type is ValueType.INT32:
        number = int(value, 0)
        if number < -0x80000000 or number > 0x7FFFFFFF:
            raise ValueError("int32 out of range")
        raw = number.to_bytes(4, byteorder="little", signed=True)
        return EncodedValue(raw=raw, display=str(number), type=value_type)
    if value_type is ValueType.UINT32:
        number = int(value, 0)
        if number < 0 or number > 0xFFFFFFFF:
            raise ValueError("uint32 out of range")
        raw = number.to_bytes(4, byteorder="little", signed=False)
        return EncodedValue(raw=raw, display=str(number), type=value_type)
    if value_type is ValueType.FLOAT:
        number = float(value)
        if math.isinf(number) or math.isnan(number):
            raise ValueError("float must be finite")
        raw = struct.pack("<f", number)
        display = f"{number:.6g}"
        return EncodedValue(raw=raw, display=display, type=value_type)
    if value_type is ValueType.ASCII:
        raw = value.encode("ascii", errors="strict")
        return EncodedValue(raw=raw, display=value, type=value_type)
    if value_type is ValueType.UTF16:
        raw = value.encode("utf-16-le")
        return EncodedValue(raw=raw, display=value, type=value_type)
    if value_type is ValueType.HEX:
        cleaned = value.replace(" ", "")
        if len(cleaned) % 2:
            cleaned = "0" + cleaned
        try:
            raw = binascii.unhexlify(cleaned)
        except (binascii.Error, ValueError) as exc:  # pragma: no cover - defensive
            raise ValueError("invalid hex literal") from exc
        return EncodedValue(raw=raw, display=cleaned.lower(), type=value_type)
    raise ValueError(f"Unsupported value type {value_type}")


def decode_value(value_type: ValueType, data: bytes) -> str:
    if value_type is ValueType.INT32:
        if len(data) < 4:
            raise ValueError("insufficient bytes for int32")
        return str(int.from_bytes(data[:4], byteorder="little", signed=True))
    if value_type is ValueType.UINT32:
        if len(data) < 4:
            raise ValueError("insufficient bytes for uint32")
        return str(int.from_bytes(data[:4], byteorder="little", signed=False))
    if value_type is ValueType.FLOAT:
        if len(data) < 4:
            raise ValueError("insufficient bytes for float")
        (number,) = struct.unpack("<f", data[:4])
        return f"{number:.6g}"
    if value_type is ValueType.ASCII:
        return data.decode("ascii", errors="replace")
    if value_type is ValueType.UTF16:
        return data.decode("utf-16-le", errors="replace")
    if value_type is ValueType.HEX:
        return data.hex()
    raise ValueError(f"Unsupported value type {value_type}")


SUPPORTED_VALUE_TYPES = {
    ValueType.INT32,
    ValueType.UINT32,
    ValueType.FLOAT,
    ValueType.ASCII,
    ValueType.UTF16,
    ValueType.HEX,
}


__all__ = [
    "EncodedValue",
    "ValueType",
    "encode_value",
    "decode_value",
    "SUPPORTED_VALUE_TYPES",
]
