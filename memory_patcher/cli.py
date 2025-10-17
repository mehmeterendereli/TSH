"""Command-driven console entry point."""

from __future__ import annotations

import cmd
from dataclasses import dataclass
from typing import Optional

from .sessions import SessionManager
from .logging_config import configure_logging


@dataclass
class ConsoleConfig:
    poll_interval_ms: int = 500


class MemoryPatcherConsole(cmd.Cmd):
    intro = "MemoryPatcher console. Type help or ? to list commands."
    prompt = "mp> "

    def __init__(self, session_manager: Optional[SessionManager] = None, config: Optional[ConsoleConfig] = None):
        super().__init__()
        self._logger = configure_logging()
        self._sessions = session_manager or SessionManager()
        self._config = config or ConsoleConfig()

    # Command implementations will be filled in later


__all__ = ["MemoryPatcherConsole", "ConsoleConfig"]
