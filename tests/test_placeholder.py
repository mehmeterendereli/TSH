from memory_patcher import MemoryPatcherConsole


def test_console_instantiates():
    console = MemoryPatcherConsole()
    assert console.prompt == "mp> "
