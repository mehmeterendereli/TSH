"""Launch the MemoryPatcher console."""

from memory_patcher import MemoryPatcherConsole


def main() -> None:
    console = MemoryPatcherConsole()
    console.cmdloop()


if __name__ == "__main__":
    main()
