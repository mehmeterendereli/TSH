"""Search and refine engine for MemoryPatcher."""

from __future__ import annotations

import logging
from dataclasses import dataclass
from typing import List

from .access import MemoryAccessor, MemoryRegion, ReadError
from .sessions import MemoryHit, SearchSession
from .types import EncodedValue

logger = logging.getLogger(__name__)


@dataclass
class ScanResult:
    hits: List[MemoryHit]
    scanned_regions: List[MemoryRegion]


class MemoryScanner:
    """Coordinate search/refine operations over remote memory."""

    def __init__(self, accessor: MemoryAccessor) -> None:
        self._accessor = accessor

    def search(self, session: SearchSession, encoded_query: EncodedValue) -> ScanResult:
        pattern = encoded_query.raw
        hits: List[MemoryHit] = []
        regions: List[MemoryRegion] = []

        for region in self._accessor.iter_regions():
            regions.append(region)
            hits.extend(self._scan_region(region, pattern, encoded_query))

        session.hits = hits
        session.raw_query = encoded_query.display
        return ScanResult(hits=hits, scanned_regions=regions)

    def refine(self, session: SearchSession, new_value: EncodedValue) -> ScanResult:
        if not session.hits:
            return self.search(session, new_value)

        updated_hits: List[MemoryHit] = []
        for hit in session.hits:
            try:
                current = self._accessor.read(hit.address, new_value.size)
            except ReadError:
                logger.debug("Read failed for address 0x%X during refine", hit.address)
                continue
            if current.startswith(new_value.raw):
                updated_hits.append(MemoryHit(address=hit.address, value=new_value))

        session.hits = updated_hits
        session.raw_query = new_value.display
        return ScanResult(hits=updated_hits, scanned_regions=[])

    def _scan_region(self, region: MemoryRegion, pattern: bytes, encoded: EncodedValue) -> List[MemoryHit]:
        if not pattern:
            return []

        hits: List[MemoryHit] = []
        overlap = len(pattern) - 1
        carry = b""

        for chunk_address, chunk in self._accessor.iter_region_chunks(region):
            buffer = carry + chunk
            search_start = 0
            while True:
                offset = buffer.find(pattern, search_start)
                if offset == -1:
                    break
                absolute_address = chunk_address - len(carry) + offset
                hits.append(MemoryHit(address=absolute_address, value=encoded))
                search_start = offset + 1

            carry = buffer[-overlap:] if overlap > 0 else b""

        return hits


__all__ = ["MemoryScanner", "ScanResult"]
