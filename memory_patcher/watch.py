"""Live watch and freeze loops for MemoryPatcher."""

from __future__ import annotations

import threading
import time
from dataclasses import dataclass
from typing import Callable, Dict, Iterable, List, Optional

from .access import MemoryAccessor
from .sessions import MemoryHit


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


class WatchManager:
    """Manage background watch/freeze threads."""

    # Implementation will be filled in later


__all__ = ["WatchManager", "WatchEntry", "FreezeEntry", "default_poll_interval_ms"]
