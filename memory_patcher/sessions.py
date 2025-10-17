"""Management of search sessions, results, and refine logic."""

from __future__ import annotations

from dataclasses import dataclass, field
from threading import RLock
from typing import Dict, Iterable, List, Optional, Sequence

from .types import ValueType, EncodedValue


@dataclass
class MemoryHit:
    address: int
    value: EncodedValue


@dataclass
class SearchSession:
    value_type: ValueType
    hits: List[MemoryHit] = field(default_factory=list)
    raw_query: str = ""


class SessionManager:
    """Hold onto search sessions keyed by process PID."""

    def __init__(self) -> None:
        self._lock = RLock()
        self._sessions: Dict[int, SearchSession] = {}

    def get(self, pid: int) -> Optional[SearchSession]:
        with self._lock:
            return self._sessions.get(pid)

    def set(self, pid: int, session: SearchSession) -> None:
        with self._lock:
            self._sessions[pid] = session

    def clear(self, pid: int) -> None:
        with self._lock:
            self._sessions.pop(pid, None)


__all__ = ["MemoryHit", "SearchSession", "SessionManager"]
