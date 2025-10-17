"""Command-driven console entry point."""

from __future__ import annotations

import cmd
import logging
import shlex
from dataclasses import dataclass
from typing import Callable, Iterable, List, Optional

from rich.console import Console
from rich.table import Table

from .access import MemoryAccessor, ReadError, WindowsMemoryAccessor
from .logging_config import configure_logging
from .scanner import MemoryScanner
from .sessions import MemoryHit, SearchSession, SessionManager
from .types import EncodedValue, ValueType, decode_value, encode_value
from .watch import FreezeEntry, WatchEntry, WatchManager, default_poll_interval_ms

LOGGER = logging.getLogger(__name__)


@dataclass
class ConsoleConfig:
    poll_interval_ms: int = default_poll_interval_ms


class MemoryPatcherConsole(cmd.Cmd):
    intro = "MemoryPatcher console. Type help or ? to list commands."
    prompt = "mp> "

    def __init__(
        self,
        session_manager: Optional[SessionManager] = None,
        config: Optional[ConsoleConfig] = None,
        accessor_factory: Optional[Callable[[int], MemoryAccessor]] = None,
    ) -> None:
        super().__init__()
        self._logger = configure_logging()
        self._sessions = session_manager or SessionManager()
        self._config = config or ConsoleConfig()
        self._console = Console(highlight=False)
        self._accessor_factory = accessor_factory or WindowsMemoryAccessor
        self._accessor: Optional[MemoryAccessor] = None
        self._scanner: Optional[MemoryScanner] = None
        self._current_pid: Optional[int] = None
        self._current_type: ValueType = ValueType.INT32
        self._watch_manager = WatchManager()
        self._watch_manager.set_interval(self._config.poll_interval_ms)

    # Helpers -----------------------------------------------------------------

    def _ensure_attached(self) -> bool:
        if self._accessor is None or self._scanner is None or self._current_pid is None:
            self._console.print("[red]No process attached. Use 'attach <pid>'.[/red]")
            return False
        return True

    def _active_session(self) -> Optional[SearchSession]:
        if self._current_pid is None:
            return None
        return self._sessions.get(self._current_pid)

    def _set_session(self, session: SearchSession) -> None:
        if self._current_pid is not None:
            self._sessions.set(self._current_pid, session)

    def _parse_indices(self, arg: str, max_index: int) -> List[int]:
        arg = arg.strip()
        if arg == "*":
            return list(range(1, max_index + 1))
        indices: List[int] = []
        for part in arg.split(","):
            part = part.strip()
            if not part:
                continue
            try:
                idx = int(part)
            except ValueError:
                raise ValueError(f"Invalid index '{part}'")
            if idx < 1 or idx > max_index:
                raise ValueError(f"Index out of range: {idx}")
            indices.append(idx)
        return sorted(set(indices))

    def _resolve_hits(self, indices: Iterable[int]) -> List[MemoryHit]:
        session = self._active_session()
        if not session:
            raise RuntimeError("No active search session")
        hits = session.hits
        return [hits[i - 1] for i in indices]

    def _cleanup(self) -> None:
        self._watch_manager.stop()
        self._watch_manager.clear_all()
        self._watch_manager.set_accessor(None)
        pid = self._current_pid
        if self._accessor is not None:
            self._accessor.close()
            self._accessor = None
        self._scanner = None
        self._current_pid = None
        if pid is not None:
            self._sessions.clear(pid)

    # Command implementations --------------------------------------------------

    def do_attach(self, arg: str) -> None:
        """attach <pid> -- open a process for inspection."""
        arg = arg.strip()
        if not arg:
            self._console.print("Usage: attach <pid>")
            return
        try:
            pid = int(arg)
        except ValueError:
            self._console.print("[red]PID must be an integer.[/red]")
            return

        if self._accessor is not None:
            self._cleanup()

        try:
            accessor = self._accessor_factory(pid)
        except Exception as exc:  # pragma: no cover - Windows specific error path
            self._console.print(f"[red]Failed to open process: {exc}[/red]")
            LOGGER.exception("Attach failed")
            return

        self._accessor = accessor
        self._scanner = MemoryScanner(accessor)
        self._current_pid = pid
        self._current_type = ValueType.INT32
        self._watch_manager.stop()
        self._watch_manager.clear_all()
        self._watch_manager.set_accessor(accessor)
        self._console.print(f"Attached to process [green]{pid}[/green].")
        LOGGER.info("Attached to process %s", pid)

    def do_detach(self, arg: str) -> None:
        """Detach from the current process."""
        if self._accessor is None:
            self._console.print("No process attached.")
            return
        self._cleanup()
        self._console.print("Detached.")

    def do_search(self, arg: str) -> None:
        """search <type> <value> -- perform an initial scan."""
        if not self._ensure_attached():
            return
        try:
            parts = shlex.split(arg)
        except ValueError as exc:
            self._console.print(f"[red]{exc}[/red]")
            return
        if len(parts) < 2:
            self._console.print("Usage: search <type> <value>")
            return
        type_token = parts[0].lower()
        value = " ".join(parts[1:])
        try:
            value_type = ValueType(type_token)
        except ValueError:
            self._console.print(f"[red]Unsupported type '{type_token}'.[/red]")
            return
        try:
            encoded = encode_value(value_type, value)
        except ValueError as exc:
            self._console.print(f"[red]{exc}[/red]")
            return

        session = SearchSession(value_type=value_type)
        result = self._scanner.search(session, encoded)  # type: ignore[union-attr]
        self._set_session(session)
        self._current_type = value_type
        self._watch_manager.clear_all()

        self._console.print(
            f"Scan complete: [cyan]{len(result.hits)}[/cyan] hits across [cyan]{len(result.scanned_regions)}[/cyan] regions."
        )
        LOGGER.info(
            "search pid=%s type=%s hits=%s regions=%s",
            self._current_pid,
            value_type.value,
            len(result.hits),
            len(result.scanned_regions),
        )

    def do_refine(self, arg: str) -> None:
        """refine <value> -- narrow current hit list with a new value."""
        if not self._ensure_attached():
            return
        session = self._active_session()
        if not session:
            self._console.print("[red]No previous search to refine.[/red]")
            return
        value = arg.strip()
        if not value:
            self._console.print("Usage: refine <value>")
            return
        try:
            encoded = encode_value(session.value_type, value)
        except ValueError as exc:
            self._console.print(f"[red]{exc}[/red]")
            return

        result = self._scanner.refine(session, encoded)  # type: ignore[union-attr]
        self._watch_manager.reindex(session.hits)
        self._console.print(f"Refine complete: [cyan]{len(result.hits)}[/cyan] hits remain.")
        LOGGER.info(
            "refine pid=%s type=%s hits=%s",
            self._current_pid,
            session.value_type.value,
            len(result.hits),
        )

    def do_results(self, arg: str) -> None:
        """results [count] -- show recent matches."""
        session = self._active_session()
        if not session:
            self._console.print("[red]No search results available.[/red]")
            return
        count = 20
        arg = arg.strip()
        if arg:
            try:
                count = max(1, int(arg))
            except ValueError:
                self._console.print("Count must be an integer.")
                return
        hits = session.hits[:count]
        table = Table(title=f"Top {len(hits)} hits (type={session.value_type.value})")
        table.add_column("#", justify="right")
        table.add_column("Address", justify="right")
        table.add_column("Encoded")
        for idx, hit in enumerate(hits, start=1):
            table.add_row(str(idx), f"0x{hit.address:016X}", hit.value.display)
        self._console.print(table)

    def do_read(self, arg: str) -> None:
        """read <indices|*> -- fetch live values for selected hits."""
        if not self._ensure_attached():
            return
        session = self._active_session()
        if not session or not session.hits:
            self._console.print("[red]No hits to read.[/red]")
            return
        arg = arg.strip() or "1"
        try:
            indices = self._parse_indices(arg, len(session.hits))
        except ValueError as exc:
            self._console.print(f"[red]{exc}[/red]")
            return
        rows = []
        for idx, hit in zip(indices, self._resolve_hits(indices)):
            try:
                data = self._accessor.read(hit.address, hit.value.size)  # type: ignore[union-attr]
            except ReadError:
                self._console.print(f"[red]Read failed for index {idx}.[/red]")
                continue
            decoded = decode_value(session.value_type, data)
            rows.append((idx, hit.address, decoded))
        if not rows:
            return
        table = Table(title="Read results")
        table.add_column("#", justify="right")
        table.add_column("Address", justify="right")
        table.add_column("Value")
        for idx, address, value in rows:
            table.add_row(str(idx), f"0x{address:016X}", value)
        self._console.print(table)

    def do_write(self, arg: str) -> None:
        """write <indices|*> <value> -- patch selected hits."""
        if not self._ensure_attached():
            return
        session = self._active_session()
        if not session or not session.hits:
            self._console.print("[red]No hits available to write.[/red]")
            return
        try:
            parts = shlex.split(arg)
        except ValueError as exc:
            self._console.print(f"[red]{exc}[/red]")
            return
        if len(parts) < 2:
            self._console.print("Usage: write <indices|*> <value>")
            return
        hit_selector = parts[0]
        value = " ".join(parts[1:])
        try:
            indices = self._parse_indices(hit_selector, len(session.hits))
        except ValueError as exc:
            self._console.print(f"[red]{exc}[/red]")
            return
        try:
            encoded = encode_value(session.value_type, value)
        except ValueError as exc:
            self._console.print(f"[red]{exc}[/red]")
            return

        for idx, hit in zip(indices, self._resolve_hits(indices)):
            try:
                self._accessor.write(hit.address, encoded.raw)  # type: ignore[union-attr]
            except Exception as exc:  # pragma: no cover - write failures are rare in tests
                self._console.print(f"[red]Write failed for index {idx}: {exc}[/red]")
                continue
            hit.value = encoded
        self._console.print(f"Patched {len(indices)} hit(s).")

    # Watch / freeze -----------------------------------------------------------

    def do_watch(self, arg: str) -> None:
        """watch add/remove/list/clear ..."""
        if not self._ensure_attached():
            return
        parts = (arg or "").split()
        if not parts:
            self._console.print("Usage: watch <add|remove|list|clear> ...")
            return
        command = parts[0].lower()
        session = self._active_session()
        if command == "list":
            table = Table(title="Watch list")
            table.add_column("#", justify="right")
            table.add_column("Address", justify="right")
            table.add_column("Last value")
            for entry in self._watch_manager.list_watches():
                table.add_row(str(entry.index), f"0x{entry.hit.address:016X}", entry.last_value.hex())
            self._console.print(table)
            return
        if command == "clear":
            self._watch_manager.clear_watches()
            self._console.print("Cleared watch list.")
            return
        if not session:
            self._console.print("[red]No active session to watch.[/red]")
            return
        if len(parts) < 2:
            self._console.print("Usage: watch add/remove <indices>")
            return
        try:
            indices = self._parse_indices(parts[1], len(session.hits))
        except ValueError as exc:
            self._console.print(f"[red]{exc}[/red]")
            return

        if command == "add":
            for idx, hit in zip(indices, self._resolve_hits(indices)):
                try:
                    initial = self._accessor.read(hit.address, hit.value.size)  # type: ignore[union-attr]
                except ReadError:
                    self._console.print(f"[red]Read failed for index {idx}.[/red]")
                    continue
                entry = WatchEntry(
                    index=idx,
                    hit=hit,
                    last_value=initial,
                    callback=self._handle_watch_update,
                )
                self._watch_manager.add_watch(entry)
            self._console.print(f"Added {len(indices)} watch entry(ies).")
        elif command == "remove":
            self._watch_manager.remove_watch(indices)
            self._console.print(f"Removed {len(indices)} watch entry(ies).")
        else:
            self._console.print("Usage: watch add/remove/list/clear ...")

    def _handle_watch_update(self, hit: MemoryHit, old: bytes, new: bytes) -> None:
        session = self._active_session()
        if not session:
            return
        value = decode_value(session.value_type, new)
        self._console.print(
            f"[yellow]Watch[/yellow] 0x{hit.address:016X}: {decode_value(session.value_type, old)} -> {value}"
        )

    def do_freeze(self, arg: str) -> None:
        """freeze add/remove/list/clear ..."""
        if not self._ensure_attached():
            return
        parts = (arg or "").split()
        if not parts:
            self._console.print("Usage: freeze <add|remove|list|clear> ...")
            return
        action = parts[0].lower()
        session = self._active_session()
        if action == "list":
            table = Table(title="Freeze list")
            table.add_column("#", justify="right")
            table.add_column("Address", justify="right")
            table.add_column("Value")
            for entry in self._watch_manager.list_freezes():
                table.add_row(str(entry.index), f"0x{entry.hit.address:016X}", entry.frozen_value.hex())
            self._console.print(table)
            return
        if action == "clear":
            self._watch_manager.clear_freezes()
            self._console.print("Cleared freezes.")
            return
        if not session:
            self._console.print("[red]No active session to freeze.[/red]")
            return
        if len(parts) < 2:
            self._console.print("Usage: freeze add/remove <indices> [value]")
            return
        try:
            indices = self._parse_indices(parts[1], len(session.hits))
        except ValueError as exc:
            self._console.print(f"[red]{exc}[/red]")
            return

        if action == "remove":
            self._watch_manager.remove_freeze(indices)
            self._console.print(f"Removed {len(indices)} freeze entry(ies).")
            return

        value = " ".join(parts[2:]) if len(parts) > 2 else session.raw_query
        try:
            encoded = encode_value(session.value_type, value)
        except ValueError as exc:
            self._console.print(f"[red]{exc}[/red]")
            return

        for idx, hit in zip(indices, self._resolve_hits(indices)):
            entry = FreezeEntry(
                index=idx,
                hit=hit,
                frozen_value=encoded.raw,
                callback=self._handle_freeze_enforced,
            )
            self._watch_manager.add_freeze(entry)
        self._console.print(f"Frozen {len(indices)} hit(s) at requested value.")

    def _handle_freeze_enforced(self, hit: MemoryHit, old: bytes, new: bytes) -> None:
        self._console.print(f"[green]Freeze[/green] 0x{hit.address:016X} reset to requested value.")

    def do_interval(self, arg: str) -> None:
        """interval <ms> -- adjust watch polling cadence."""
        arg = arg.strip()
        if not arg:
            self._console.print("Usage: interval <ms>")
            return
        try:
            value = int(arg)
        except ValueError:
            self._console.print("Polling interval must be an integer (ms).")
            return
        self._watch_manager.set_interval(value)
        self._console.print(f"Polling interval set to {value} ms.")

    def do_status(self, arg: str) -> None:
        """status -- show current attachment, hits, watch/freeze counts."""
        pid = self._current_pid or 0
        session = self._active_session()
        hits = len(session.hits) if session else 0
        watches = len(list(self._watch_manager.list_watches()))
        freezes = len(list(self._watch_manager.list_freezes()))
        self._console.print(
            f"pid={pid} hits={hits} watches={watches} freezes={freezes} type={self._current_type.value}"
        )

    def do_intervalms(self, arg: str) -> None:  # alias
        self.do_interval(arg)

    def do_exit(self, arg: str) -> bool:  # type: ignore[override]
        self._cleanup()
        self._console.print("Goodbye.")
        return True

    def do_EOF(self, arg: str) -> bool:  # type: ignore[override]
        self._console.print("")
        return self.do_exit(arg)

    def postloop(self) -> None:
        self._cleanup()

    # Completion helpers ------------------------------------------------------

    def complete_watch(self, text: str, line: str, begidx: int, endidx: int):  # pragma: no cover - CLI sugar
        return [opt for opt in ["add", "remove", "list", "clear"] if opt.startswith(text)]

    def complete_freeze(self, text: str, line: str, begidx: int, endidx: int):  # pragma: no cover
        return [opt for opt in ["add", "remove", "list", "clear"] if opt.startswith(text)]


__all__ = ["MemoryPatcherConsole", "ConsoleConfig"]
