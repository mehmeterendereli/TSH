from memory_patcher.access import LocalMemoryAccessor
from memory_patcher.scanner import MemoryScanner
from memory_patcher.sessions import SearchSession
from memory_patcher.types import ValueType, encode_value


def make_accessor() -> LocalMemoryAccessor:
    size = 0x30000
    backing = bytearray(size)
    pattern = encode_value(ValueType.ASCII, "HELLO").raw
    positions = [0x100, 0x1FFFE, 0x2FF00]
    for pos in positions:
        backing[pos : pos + len(pattern)] = pattern
    return LocalMemoryAccessor(backing)


def test_search_detects_cross_chunk_hits():
    accessor = make_accessor()
    scanner = MemoryScanner(accessor)
    value = encode_value(ValueType.ASCII, "HELLO")
    session = SearchSession(value_type=ValueType.ASCII)
    result = scanner.search(session, value)
    addresses = {hit.address for hit in result.hits}
    assert {0x100, 0x1FFFE, 0x2FF00} <= addresses


def test_refine_reduces_hits():
    accessor = make_accessor()
    scanner = MemoryScanner(accessor)
    initial_value = encode_value(ValueType.ASCII, "HELLO")
    session = SearchSession(value_type=ValueType.ASCII)
    scanner.search(session, initial_value)

    target_address = session.hits[1].address
    new_value = encode_value(ValueType.ASCII, "WORLD")
    accessor.write(target_address, new_value.raw)

    result = scanner.refine(session, new_value)
    assert len(result.hits) == 1
    assert result.hits[0].address == target_address
