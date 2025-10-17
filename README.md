# TSH Native Diagnostics

TSH Native Diagnostics is an educational Windows x64 memory introspection toolkit implemented entirely in C++ and kernel-mode C. It demonstrates secure driver communication, pointer analysis, pattern scans, and live telemetry for processes that the user owns.

## Highlights
- Pure native stack: Win32/ImGui-friendly user-mode front end and WDM-style kernel back end.
- Protected driver channel: custom IOCTL protocol over DeviceIoControl, shared payload definitions, and rigorous validation.
- Driver-mediated memory access: chunked scans, pointer tracing, monitor snapshots, and patch writes are serviced via kernel IOCTLs with secure fallbacks.
- Memory tooling primitives: process enumeration, pattern scanning, result refinement, pointer chain scaffolding, and live value monitoring hooks.
- Extensible instrumentation: hooks for optional self-process patching with reversible trampolines.
- Optional Dear ImGui shell: launch `tsh_user.exe --imgui` (with `-DTSH_ENABLE_IMGUI=ON`) for a native renderer scaffold.
- Auditable design: clear separation between privileged operations and UI logic, with space for ETW telemetry and logging.

## How It Works

1. **Attach & Verify** â€“ `ProcessManager` enumerates processes, the driver validates ownership or `SeDebugPrivilege` before servicing requests.
2. **Broker All Memory I/O** â€“ `MemoryAccessor` funnels reads/writes/PTR traces through the driver (`IOCTL_TSH_*`). Safe Win32 fallbacks keep the tools usable without the driver.
3. **Analyse & Monitor** â€“ `PatternScanner` performs chunked scans with overlap handling, `PointerResolver` follows multi-level chains, and `ValueMonitor` polls addresses (CLI table + ImGui graphs).
4. **Instrument Responsibly** â€“ `HookController` stages reversible patches, handing payloads to the kernel which applies them via `MmCopyVirtualMemory`.
5. **Iterate via Sessions** â€“ `ScanSession` caches region metadata and the canonical pattern so `refine` only touches surviving hits.

## Architecture at a Glance

| Layer               | Responsibilities                                                                                                               |
|---------------------|--------------------------------------------------------------------------------------------------------------------------------|
| **User (CLI/ImGui)**| Process attach, memory scanning, pointer tracing, monitor snapshots, patch orchestration, and UI bindings (ImGui panels).      |
| **Shared**          | Protocol structures/typedefs, IOCTL constants, helper macros (`TSH_MIN`, `TSH_MONITOR_SAMPLE_MAX_BYTES`).                      |
| **Kernel**          | IOCTL dispatch, region enumeration, pointer walking, monitor snapshots, patch execution, and ownership/privilege enforcement. |

## Driver Capabilities

| IOCTL                       | Purpose                                      | Safeguards & Notes                                                  |
|-----------------------------|-----------------------------------------------|---------------------------------------------------------------------|
| `IOCTL_TSH_QUERY_REGIONS`   | Enumerate committed VADs                      | Ownership check, `ZwQueryVirtualMemory` traversal                   |
| `IOCTL_TSH_READ_MEMORY`     | Read arbitrary user memory                    | `MmCopyVirtualMemory` copy-out, bounded to caller buffer            |
| `IOCTL_TSH_POINTER_TRACE`   | Follow pointer chains (base + offsets)       | Validates depth/offsets, returns partial chain if a hop fails       |
| `IOCTL_TSH_MONITOR_CONTROL` | Snapshot N addresses (â‰¤64 bytes each)         | Throttles entry count, emits per-address NTSTATUS + captured size   |
| `IOCTL_TSH_PATCH_REQUEST`   | Apply patch payload to self-owned processes   | Requires write/operation access; returns bytes written + NTSTATUS   |

All protocol structs have C typedefs (`PTSH_*`) and C++ wrappers (`Protocol.hpp`) to keep serialization simple and consistent.

## Educational Walkthrough

Need a concrete example? See [`docs/usage-guide.md`](docs/usage-guide.md) for a step-by-step scenario that:

1. Types `mehmet` inside Notepad.
2. Runs UTF-16 scans and refines after edits.
3. Uses `monitor` and `patch` to observe/edit the buffer live.
4. Repeats the workflow inside the ImGui front end (pointer table, monitor graph, patch widgets).

Following that walkthrough once will familiarise you with the end-to-end tooling.

## Repository Layout
- CMakeLists.txt - root build script orchestrating user, driver, and test targets.
- build/ - helper scripts and (optional) out-of-source build tree.
- build.bat - configure and build both components with CMake.
- build_driver.bat - rebuild only the kernel target after configuration.
- include/
- shared/ - IOCTL codes, protocol structs, and cross-layer helpers.
- user/ - user-mode interfaces (driver channel, process manager, scanners, monitors, instrumentation helpers).
- kernel/ - driver-side helper declarations for IOCTL dispatchers.
- docs/usage-guide.md - detailed CLI/ImGui walkthrough (Notepad `mehmet` example).
- src/user/ - Win32 entry point, communications layer, and native engine modules (Core/, Analysis/, Monitoring/, Instrumentation/).
- src/kernel/ - WDM driver skeleton plus modular subsystems (Memory/, Scan/, Pointer/, Monitor/, Instrumentation/).
- tests/ - placeholder CMake target for future unit and integration suites.
- docs/ - design notes, threat model, and developer guides (to be populated).
- tools/ - deployment scripts and diagnostic utilities (to be populated).

## CLI Commands
- `attach <pid>`: bind to a process you own and prime the driver channel.
- `scan <type> <value>`: run the initial sweep (types: int32, uint32, float, ascii, utf16, bytes).
- `refine <value>`: filter the existing hit list with a new value.
- `results [count]`: dump the first `count` hits with live values via the driver accessor.
- `pointer <addr> <offsets...>`: resolve multi-level pointer chains (hex or decimal addresses/offsets).
- `monitor <addr[:size],...>`: snapshot addresses via IOCTL telemetry (size defaults to 4 bytes).
- `patch <addr> <hex-bytes>`: emit a patch request through the privileged driver path.

Driver features require the privileged channel; when the WDK is unavailable, the build skips the kernel project and the CLI automatically falls back to Win32 APIs for read/monitor operations.

## ImGui Shell
1. Configure with Dear ImGui sources available and enable the flag: `cmake -DTSH_ENABLE_IMGUI=ON ...`.
2. Launch `tsh_user.exe --imgui` to open the experimental UI.
3. Panels: **Processes** (attach), **Pointer Trace**, **Monitor**, and **Patch** mirror the CLI functionality, plotting monitor bytes with `ImGui::PlotLines` and tabulating pointer chains.
4. Integrate your preferred renderer/platform backend (e.g., Win32 + DirectX11) before deployment; the stub ships a single-frame loop to keep the sample self-contained.
## Building
1. Open an **x64 Native Tools Command Prompt for VS 2022** with the Windows Driver Kit **10.0.26100** environment available (the overrides in `src/kernel/CMakeLists.txt` assume the default `C:/Program Files (x86)/Windows Kits/10` layout).
2. Configure once:  
   `cmake -S . -B build/vs -A x64`
3. Build the user-mode and kernel artefacts in **Release** mode (kernel drivers cannot link against the MSVC debug runtime):
   ```cmd
   cmake --build build/vs --config Release --target TSH.Driver
   cmake --build build/vs --config Release --target TSH.User
   ```
4. Optional (user-mode debugging only): `build.bat` or `cmake --build build/vs --config Debug --target TSH.User`
5. Optional (ImGui): enable with `cmake -DTSH_ENABLE_IMGUI=ON ...` and provide Dear ImGui plus a renderer backend under `external/imgui/`.

The driver binary lands in `build/vs/driver/Release/tsh_driver.sys`. Sign it or enable test-signing before loading with `sc create` / `sc start`.

> Producing a production-ready `.sys` still requires the full WDK toolchain, updated INF/CAT packaging, and appropriate signatures. Adjust the hard-coded Kits paths if your installation uses a different version.

## Testing & Validation
- `cmake --build build/vs --config Release --target TSH.User` – rebuild the CLI after code changes.
- `cmake --build build/vs --config Release --target TSH.Driver` – regenerate the driver (Release configuration only).
- `cmake --build build/vs --config Release --target TSH.Driver -- /t:Clean` – clean kernel artefacts before a fresh build.
- `tsh_user.exe --imgui --imgui-backend` – once a renderer backend is wired in, bring up the visual tooling.

For automated regression, consider adding Catch2/GoogleTest targets under `tests/` or scripting CLI sessions that assert on the textual output produced by each command.




