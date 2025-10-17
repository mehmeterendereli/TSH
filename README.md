# MemoryPatcher

MemoryPatcher is a Windows x64 training utility for exploring live process memory in a controlled way. Attach to a target, scan for integers or strings, refine the hit list as a value changes, and monitor or freeze individual addresses with millisecond resolution. A paired kernel-mode driver keeps privileged operations reliable and quiet, while every action is logged.

## Key Capabilities

- **Interactive console workflow** – attach, search, refine, read, write, watch, and freeze from short commands (`search`, `refine`, `watch add`, …).
- **Value refine / next scan** – re-run the last search against hits only, tightening in on dynamic values in seconds.
- **Live watch + freeze** – track selected addresses in real time or pin them to a fixed value with automatic re-write.
- **Kernel assisted access** – optional `MemoryPatcherDrv.sys` driver (source included) performs reads, writes, and protection changes beyond user-mode limitations.
- **Stealth helpers** – direct syscall path, anti-debug monitoring, and configurable timing scatter keep actions harder to trace.
- **Comprehensive logging** – all searches, reads, writes, freezes, and driver activity are recorded under `logs/` for later review.

## Build Instructions

1. **User-mode console**
   ```cmd
   build.bat
   ```
   Produces `build\MemoryPatcher.exe` using the latest Visual Studio Build Tools located via `vswhere`.

2. **Kernel driver (optional but recommended)**
   ```cmd
   build_driver.bat
   ```
   Run from the "x64 Native Tools Command Prompt for VS 2022" with the Windows Driver Kit (WDK) environment initialised. The script emits `build\driver\MemoryPatcherDrv.sys`.

Copy both the executable and driver into the same directory when deploying (the loader expects `MemoryPatcherDrv.sys` next to the console binary).

## Running the Console

```cmd
MemoryPatcher.exe
```

At start-up the tool prints the banner and the full command list. Enter commands at the `mp>` prompt:

| Command | Purpose |
|---------|---------|
| `attach [pid]` | Attach via list or PID. |
| `search <i|ascii|utf16> <value>` | Full memory scan. |
| `refine <value>` | Re-filter previous hits with a new value (next scan). |
| `results [count]` | Show latest matches. |
| `read <indices|*>` | Refresh hit values. |
| `write <indices|*> <value>` | Patch one or many hits. |
| `watch add/remove/list/clear …` | Manage live watch list (values printed on change). |
| `freeze add/remove/list/clear …` | Pin addresses to a value (auto rewrite on change). |
| `interval <ms>` | Set watch/freeze polling cadence (default 500 ms). |
| `settings <stealth|antidebug|terminate> <on/off>` | Adjust stealth subsystems. |
| `status` | Summaries for attachment, result count, watch/freeze totals. |
| `help` | Show the full command reference. |
| `exit` | Leave the console. |

### Watching and Freezing

- **watch add 1,2** – refreshes the selected hits on every polling tick and prints when a value changes.
- **freeze add 3 9999** – writes the value immediately and re-applies it whenever the target overwrites memory.
- Watches and freezes are independent; both can run side-by-side. Use `interval` to increase the sampling rate when needed.

## Kernel Driver Integration

- The user-mode binary tries to load `MemoryPatcherDrv.sys` automatically on the first privileged operation. Installation requires administrative rights.
- If the driver is absent or cannot be loaded, MemoryPatcher falls back to direct syscalls and Win32 APIs (with reduced access on protected pages).
- The driver exposes IOCTLs for reading, writing, and adjusting protection using `MmCopyVirtualMemory`, keeping the user-mode footprint small.

## Logging

Every session creates a UTC timestamped log file under `logs/`. Searches, refines, reads, writes, watch updates, freeze rewrites, settings changes, and driver load events are recorded for auditing.

## Requirements

- Windows 10 / 11 x64.
- Visual Studio 2022 Build Tools (for `build.bat`).
- Windows Driver Kit 10 (for `build_driver.bat`).
- Administrator rights recommended for driver installation and SE_DEBUG privilege escalation.

## Responsible Use

MemoryPatcher is provided for educational analysis on systems and software you are authorised to inspect. Respect licences, terms of service, and local legislation at all times.
