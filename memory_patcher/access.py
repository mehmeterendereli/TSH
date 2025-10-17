"""Windows process access abstraction for MemoryPatcher."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, Protocol, Sequence


class ReadError(RuntimeError):
    """Raised when a memory read operation fails permanently."""


class WriteError(RuntimeError):
    """Raised when a memory write operation fails permanently."""


@dataclass(frozen=True)
class MemoryRegion:
    base_address: int
    size: int
    protection: str


class MemoryAccessor(Protocol):
    """Protocol for reading and writing remote process memory."""

    pid: int

    def close(self) -> None: ...

    def iter_regions(self) -> Iterable[MemoryRegion]: ...

    def read(self, address: int, size: int) -> bytes: ...

    def write(self, address: int, data: bytes) -> None: ...


__all__ = [
    "MemoryRegion",
    "MemoryAccessor",
    "ReadError",
    "WriteError",
]
