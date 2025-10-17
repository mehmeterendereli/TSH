"""Value encoding helpers and supported search types."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import Any, Callable


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


Encoder = Callable[[Any], EncodedValue]


__all__ = ["ValueType", "EncodedValue", "Encoder"]
