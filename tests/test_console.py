import io
import time

from memory_patcher.access import LocalMemoryAccessor
from memory_patcher.cli import ConsoleConfig, MemoryPatcherConsole
from memory_patcher.sessions import SessionManager
from memory_patcher.types import ValueType, decode_value, encode_value


class FakeAccessorFactory:
    def __init__(self, backing_map):
        self._backing_map = backing_map

    def __call__(self, pid: int) -> LocalMemoryAccessor:
        if pid not in self._backing_map:
            raise RuntimeError("Unknown pid")
        return LocalMemoryAccessor(self._backing_map[pid])


def test_console_flow_exercises_commands():
    pid = 4242
    backing = bytearray(2048)
    int_value = encode_value(ValueType.INT32, "123")
    backing[128:132] = int_value.raw
    backing[256:260] = int_value.raw

    factory = FakeAccessorFactory({pid: backing})
    console = MemoryPatcherConsole(
        session_manager=SessionManager(),
        config=ConsoleConfig(poll_interval_ms=20),
        accessor_factory=factory,
    )
    buffer = io.StringIO()
    console._console = console._console.__class__(file=buffer, force_terminal=False, color_system=None)  # type: ignore[attr-defined]

    console.onecmd(f"attach {pid}")
    console.onecmd("search int32 123")
    session = console._active_session()
    assert session is not None
    assert len(session.hits) == 2

    console.onecmd("results 2")
    console.onecmd("read 1,2")

    console.onecmd("write 1 456")
    console.onecmd("write 2 456")

    updated_value = decode_value(ValueType.INT32, backing[128:132])
    assert updated_value == "456"

    console.onecmd("watch add 1,2")
    time.sleep(0.05)

    console.onecmd("freeze add 1 777")
    backing[128:132] = encode_value(ValueType.INT32, "12").raw
    time.sleep(0.15)
    assert decode_value(ValueType.INT32, backing[128:132]) == "777"

    backing[256:260] = encode_value(ValueType.INT32, "123").raw
    console.onecmd("refine 777")
    time.sleep(0.05)
    watch_entries = list(console._watch_manager.list_watches())
    assert len(watch_entries) == 1
    assert watch_entries[0].index == 1

    console.onecmd("watch list")
    console.onecmd("freeze list")
    console.onecmd("interval 100")
    console.onecmd("status")
    console.onecmd("freeze clear")
    console.onecmd("watch clear")
    console.onecmd("detach")
    console.onecmd("exit")

    console._watch_manager.stop()
