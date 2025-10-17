# MemoryPatcher

MemoryPatcher is a Windows 10/11 x64 training utility written in Python. Attach to a target process, locate interesting values with full memory scans, refine the hit list as values change, and keep a real time watch or freeze running on selected addresses. An optional kernel-mode driver is included for privileged read/write operations when user mode APIs are blocked.

## Highlights
- **Command driven console** – `attach`, `search`, `refine`, `read`, `write`, `watch`, and `freeze` are one-liners.
- **Value refinement** – repeat the last search against the current hit list to isolate dynamic values quickly.
- **Live watch & freeze** – poll addresses at millisecond cadence, print changes, or automatically restore a frozen value.
- **Driver aware** – when `MemoryPatcherDrv.sys` is present, the console routes reads/writes through the kernel bridge and falls back to Win32 APIs if anything fails.
- **Structured logging** – every operation is logged under `logs/memorypatcher.log` for later review.
- **Test covered** – unit and integration tests exercise the scanner core, watch manager, and console flow (`python -m pytest`).

## Repository Layout
- `memory_patcher/` – user-mode library (access layer, scanner, console, watch manager, driver bridge).
- `scripts/run_console.py` – convenience launcher for the interactive console.
- `tests/` – pytest suite covering encoding, scanning, watch/freeze loops, and the CLI workflow.
- `driver/` – kernel driver sources (`MemoryPatcherDrv.c`, `MemoryPatcherIoctl.h`, Visual Studio project).
- `build.bat` – bootstrap virtual environment installation and run the full test suite.
- `build_driver.bat` – invoke MSBuild to compile the kernel driver (requires the WDK command prompt).

## Quick Start
```cmd
python -m pip install -e .[dev]
python scripts\run_console.py
```
At the `mp>` prompt use commands such as:
```
attach 1234
search int32 1500
results 10
watch add 1,2
freeze add 1 9999
interval 100
status
```
Type `help` or `?` to see all commands. `exit` or `Ctrl+Z` leaves the console.

### Search Types
- `int32`, `uint32`, `float`
- `ascii` – raw ASCII strings
- `utf16` – UTF-16LE text
- `hex` – byte patterns (accepts whitespace)

### Watch and Freeze
- `watch add <indices>` – begin polling the selected hits. Changes are printed immediately.
- `freeze add <indices> <value>` – force a value and automatically rewrite when the target mutates it.
- `watch list`, `freeze list` – view active entries.
- `watch clear`, `freeze clear` – stop monitoring/freeze loops.
- `interval <ms>` – adjust the polling cadence (default 500 ms).

## Running Tests
```cmd
python -m pytest
```
Coverage reports are emitted automatically because pytest-cov is configured in `pyproject.toml`.

## Kernel Driver
1. Open an **x64 Native Tools Command Prompt for VS 2022** with the Windows Driver Kit initialised.
2. Run `build_driver.bat`. The signed binary lands in `driver\build\driver\MemoryPatcherDrv.sys`.
3. Install the driver in test mode, for example:
   ```cmd
   sc create MemoryPatcherDrv type= kernel binPath= C:\path\to\MemoryPatcherDrv.sys
   sc start MemoryPatcherDrv
   ```
4. Launch the console. When the driver is reachable (`\\.\MemoryPatcher`), reads and writes are issued through the bridge before falling back to Win32 APIs.

> **Note**: The driver uses `MmCopyVirtualMemory` to copy between the caller and the target process. Administrative rights and test-signing mode are required during development.

## Logging
`memory_patcher.logging_config.configure_logging` creates `logs/memorypatcher.log`. Search, refine, read, write, watch, freeze, and driver fallback events are timestamped for auditing.

## Responsible Use
MemoryPatcher is intended for controlled training and authorised research on systems you own or are explicitly permitted to analyse. Respect software licences, terms of service, and local laws.
