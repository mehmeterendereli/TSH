# TSH Native Diagnostics

TSH Native Diagnostics is an educational Windows x64 memory introspection toolkit implemented entirely in C++ and kernel-mode C. It demonstrates secure driver communication, pointer analysis, pattern scans, and live telemetry for processes that the user owns.

## Highlights
- Pure native stack: Win32/ImGui-friendly user-mode front end and WDM-style kernel back end.
- Protected driver channel: custom IOCTL protocol over DeviceIoControl, shared payload definitions, and rigorous validation.
- Memory tooling primitives: process enumeration, pattern scanning, result refinement, pointer chain scaffolding, and live value monitoring hooks.
- Extensible instrumentation: hooks for optional self-process patching with reversible trampolines.
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

## Building
1. Open an x64 Native Tools Command Prompt for VS 2022 with the Windows Driver Kit environment configured.
2. Run build.bat. The script configures CMake under build\vs and compiles the user executable and driver library in Debug mode.
3. To rebuild the driver target for Release, execute build_driver.bat after the initial configuration.

> Note: Producing a loadable .sys requires the WDK toolset, driver signing certificates, and additional linker flags that will be incorporated as the kernel feature set matures.

## Current Status
- User mode now exposes ProcessManager, PatternScanner, PointerResolver, ScanSession, ResultRefiner, ValueMonitor, and HookController scaffolding.
- Kernel mode is partitioned into Memory, Scan, Pointer, Monitor, and Instrumentation handlers with IOCTL routing in place.
- Shared protocol definitions enumerate future request and response payloads for memory regions, scans, pointer traces, monitors, and patch operations.

## Next Implementation Steps
1. Implement real scan pipelines (typed comparers, asynchronous chunking) and wire them into a CLI or ImGui interface.
2. Flesh out kernel subsystems with guarded access checks, paging-aware traversals, and ETW logging.
3. Define end-to-end IOCTL payloads for scan requests, pointer queries, and monitor streams; add validation, auditing, and throttling logic.
4. Introduce native unit tests (user and driver) and scripted deployment helpers under tools/.

## Responsible Use
Operate the toolkit only on systems and processes you own or are explicitly authorised to inspect. Always follow platform security guidelines, licensing terms, and local legislation.
