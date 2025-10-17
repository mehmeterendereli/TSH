"""Search and refine engine for MemoryPatcher."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, List, Sequence

from .access import MemoryAccessor, MemoryRegion
from .sessions import MemoryHit, SearchSession
from .types import EncodedValue, ValueType


@dataclass
class ScanResult:
    hits: List[MemoryHit]
    scanned_regions: List[MemoryRegion]


class MemoryScanner:
    """Coordinate search/refine operations over remote memory."""

    def __init__(self, accessor: MemoryAccessor) -> None:
        self._accessor = accessor

    def search(self, session: SearchSession, encoded_query: EncodedValue) -> ScanResult:
        raise NotImplementedError

    def refine(self, session: SearchSession, new_value: EncodedValue) -> ScanResult:
        raise NotImplementedError


__all__ = ["MemoryScanner", "ScanResult"]
