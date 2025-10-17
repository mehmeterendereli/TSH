import time

from memory_patcher.access import LocalMemoryAccessor
from memory_patcher.sessions import MemoryHit
from memory_patcher.types import ValueType, decode_value, encode_value
from memory_patcher.watch import FreezeEntry, WatchEntry, WatchManager


def test_watch_and_freeze_cycle():
    backing = bytearray(16)
    initial = encode_value(ValueType.INT32, "100")
    backing[0:4] = initial.raw
    accessor = LocalMemoryAccessor(backing)

    manager = WatchManager()
    manager.set_accessor(accessor)
    manager.set_interval(20)

    hit = MemoryHit(address=0, value=initial)
    events = []

    def on_watch(hit_obj, old, new):
        events.append((decode_value(ValueType.INT32, old), decode_value(ValueType.INT32, new)))

    watch_entry = WatchEntry(index=1, hit=hit, last_value=initial.raw, callback=on_watch)
    manager.add_watch(watch_entry)

    updated = encode_value(ValueType.INT32, "150")
    accessor.write(0, updated.raw)
    time.sleep(0.1)

    assert ("100", "150") in events

    freeze_value = encode_value(ValueType.INT32, "777")
    freeze_entry = FreezeEntry(index=1, hit=hit, frozen_value=freeze_value.raw)
    manager.add_freeze(freeze_entry)

    accessor.write(0, encode_value(ValueType.INT32, "5").raw)
    time.sleep(0.15)
    final_bytes = accessor.read(0, 4)
    assert decode_value(ValueType.INT32, final_bytes) == "777"

    manager.reindex([hit])
    manager.clear_watches()
    manager.clear_freezes()
    manager.stop()
