"""Live watch and freeze loops for MemoryPatcher."""

from __future__ import annotations

import threading
import time
from dataclasses import dataclass, field
from typing import Callable, Dict, Iterable, Iterator, Optional

from .access import MemoryAccessor, ReadError, WriteError
from .sessions import MemoryHit

__all__ = [
    "WatchManager",
    "WatchEntry",
    "FreezeEntry",
    "default_poll_interval_ms",
]


default_poll_interval_ms = 500


@dataclass
class WatchEntry:
    index: int
    hit: MemoryHit
    last_value: bytes
    callback: Callable[[MemoryHit, bytes, bytes], None]


@dataclass
class FreezeEntry:
    index: int
    hit: MemoryHit
    frozen_value: bytes
    callback: Optional[Callable[[MemoryHit, bytes, bytes], None]] = None


class WatchManager:
    """Manage background watch/freeze threads."""

    def __init__(self) -> None:
        self._accessor: Optional[MemoryAccessor] = None
        self._lock = threading.RLock()
        self._watches: Dict[int, WatchEntry] = {}
        self._freezes: Dict[int, FreezeEntry] = {}
        self._interval_ms = default_poll_interval_ms
        self._thread: Optional[threading.Thread] = None
        self._stop_event = threading.Event()

    # Lifecycle -----------------------------------------------------------------

    def set_accessor(self, accessor: Optional[MemoryAccessor]) -> None:
        with self._lock:
            self._accessor = accessor

    def set_interval(self, interval_ms: int) -> None:
        with self._lock:
            self._interval_ms = max(50, interval_ms)

    def clear_watches(self) -> None:
        with self._lock:
            self._watches.clear()
        self._ensure_thread_state()

    def clear_freezes(self) -> None:
        with self._lock:
            self._freezes.clear()
        self._ensure_thread_state()

    def clear_all(self) -> None:
        with self._lock:
            self._watches.clear()
            self._freezes.clear()
        self._ensure_thread_state()

    def reindex(self, hits: Iterable[MemoryHit]) -> None:
        with self._lock:
            address_to_index = {hit.address: i for i, hit in enumerate(hits, start=1)}
            new_watches: Dict[int, WatchEntry] = {}
            for entry in self._watches.values():
                new_idx = address_to_index.get(entry.hit.address)
                if new_idx is None:
                    continue
                entry.index = new_idx
                new_watches[new_idx] = entry
            self._watches = new_watches

            new_freezes: Dict[int, FreezeEntry] = {}
            for entry in self._freezes.values():
                new_idx = address_to_index.get(entry.hit.address)
                if new_idx is None:
                    continue
                entry.index = new_idx
                new_freezes[new_idx] = entry
            self._freezes = new_freezes
        self._ensure_thread_state()

    def stop(self) -> None:
        self._stop_event.set()
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=2.0)
        self._thread = None
        self._stop_event.clear()

    # Watch management ---------------------------------------------------------

    def add_watch(self, entry: WatchEntry) -> None:
        with self._lock:
            self._watches[entry.index] = entry
            self._ensure_thread_state()

    def remove_watch(self, indices: Iterable[int]) -> None:
        with self._lock:
            for idx in indices:
                self._watches.pop(idx, None)
            self._ensure_thread_state()

    def list_watches(self) -> Iterator[WatchEntry]:
        with self._lock:
            return iter(list(self._watches.values()))

    # Freeze management --------------------------------------------------------

    def add_freeze(self, entry: FreezeEntry) -> None:
        with self._lock:
            self._freezes[entry.index] = entry
            self._ensure_thread_state()

    def remove_freeze(self, indices: Iterable[int]) -> None:
        with self._lock:
            for idx in indices:
                self._freezes.pop(idx, None)
            self._ensure_thread_state()

    def list_freezes(self) -> Iterator[FreezeEntry]:
        with self._lock:
            return iter(list(self._freezes.values()))

    # Thread loop --------------------------------------------------------------

    def _ensure_thread_state(self) -> None:
        should_run = self._watches or self._freezes
        if should_run and (self._thread is None or not self._thread.is_alive()):
            self._stop_event.clear()
            self._thread = threading.Thread(target=self._run_loop, daemon=True, name="MemoryWatcher")
            self._thread.start()
        elif not should_run and self._thread and self._thread.is_alive():
            self._stop_event.set()
            self._thread.join(timeout=1.0)
            self._thread = None
            self._stop_event.clear()

    def _run_loop(self) -> None:
        while not self._stop_event.is_set():
            with self._lock:
                accessor = self._accessor
                watches = list(self._watches.values())
                freezes = list(self._freezes.values())
                interval_ms = self._interval_ms
            if accessor is None:
                time.sleep(0.25)
                continue

            for watch in watches:
                try:
                    data = accessor.read(watch.hit.address, len(watch.last_value))
                except ReadError:
                    continue
                if data != watch.last_value:
                    old = watch.last_value
                    watch.last_value = data
                    try:
                        watch.callback(watch.hit, old, data)
                    except Exception:  # pragma: no cover - defensive
                        pass

            for freeze in freezes:
                try:
                    current = accessor.read(freeze.hit.address, len(freeze.frozen_value))
                except ReadError:
                    continue
                if current != freeze.frozen_value:
                    try:
                        accessor.write(freeze.hit.address, freeze.frozen_value)
                    except WriteError:
                        continue
                    if freeze.callback:
                        try:
                            freeze.callback(freeze.hit, current, freeze.frozen_value)
                        except Exception:  # pragma: no cover
                            pass

            time.sleep(max(0.05, interval_ms / 1000.0))
