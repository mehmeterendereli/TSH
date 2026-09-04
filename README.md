# TSH Native Diagnostics

> **Archived security-research prototype. This repository is not maintained, supported, or production-ready.**

TSH is a Windows x64 memory-introspection experiment containing a user-mode C++ client and a kernel-driver scaffold. It explores process enumeration, memory reads, scans, pointer tracing, monitoring, and write/patch requests through an IOCTL boundary.

## Safety boundary

- Kernel drivers and cross-process memory operations can destabilize a system or weaken its security.
- No signed driver, installer, release artifact, or current automated test evidence is provided.
- Do not load the driver on a production computer or use it against software or systems you do not own and have explicit permission to test.
- A production driver would require a complete threat review, least-privilege design, INF/CAT packaging, appropriate signing, and controlled validation on disposable test systems.

The source remains online for historical review only. Build notes under `docs/` describe the original experiment; their commands and fixed toolchain assumptions were not revalidated during archival.

## License

No license file is present. Public visibility does not grant permission to copy, modify, or redistribute the source.
