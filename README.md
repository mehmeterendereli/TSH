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

## Repository Layout
- CMakeLists.txt - root build script orchestrating user, driver, and test targets.
- build/ - helper scripts and (optional) out-of-source build tree.
- build.bat - configure and build both components with CMake.
- build_driver.bat - rebuild only the kernel target after configuration.
- include/
- shared/ - IOCTL codes, protocol structs, and cross-layer helpers.
- user/ - user-mode interfaces (driver channel, process manager, scanners, monitors, instrumentation helpers).
- kernel/ - driver-side helper declarations for IOCTL dispatchers.
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
1. Open an x64 Native Tools Command Prompt for VS 2022 with the Windows Driver Kit environment configured.
2. Run build.bat. The script configures CMake under build\vs and compiles the user executable and driver library in Debug mode.
3. To rebuild the driver target for Release, execute build_driver.bat after the initial configuration.
4. (Optional) Enable the ImGui shell with `cmake -DTSH_ENABLE_IMGUI=ON` and place the Dear ImGui sources under `external/imgui/`.

> Note: Producing a loadable .sys requires the WDK toolset, driver signing certificates, and additional linker flags that will be incorporated as the kernel feature set matures.

## Current Status
- Interactive CLI shell supports process enumeration, driver status, attach, scan, refine, and result inspection workflows.
- Chunked pattern scanner handles byte, integer, float, and string comparisons across driver-fed or Win32 enumerated memory regions.
- Kernel driver services region queries with privilege validation, `ZwQueryVirtualMemory` traversal, and debug trace logging hooks.
- Shared protocol layer defines IOCTL contracts for regions, scans, pointer traces, monitors, and patch operations.

## Next Implementation Steps
1. Extend kernel handlers for pattern scans, pointer tracing, monitor subscriptions, and patch orchestration with audit trails.
2. Replace Win32 `ReadProcessMemory` usage with driver-mediated transfers for high integrity and guard-page aware streaming.
3. Layer an ImGui front end (optional) over the CLI core for richer visualisation of regions, hits, and monitors.
4. Add native unit/integration tests plus ETW consumer tooling under `tests/` and `tools/`.

## Responsible Use
Operate the toolkit only on systems and processes you own or are explicitly authorised to inspect. Always follow platform security guidelines, licensing terms, and local legislation.
