"""Windows process access abstraction for MemoryPatcher."""

from __future__ import annotations

import ctypes
import logging
import threading
from dataclasses import dataclass
from typing import TYPE_CHECKING, Iterable, Iterator, Optional, Protocol

try:  # pragma: no cover - runtime guard for non-Windows systems
    from ctypes import wintypes
except ImportError:  # pragma: no cover
    wintypes = None  # type: ignore[assignment]


class ReadError(RuntimeError):
    """Raised when a memory read operation fails permanently."""


class WriteError(RuntimeError):
    """Raised when a memory write operation fails permanently."""


@dataclass(frozen=True)
class MemoryRegion:
    base_address: int
    size: int
    protection: str


class MemoryAccessor(Protocol):
    """Protocol for reading and writing remote process memory."""

    pid: int

    def close(self) -> None: ...

    def iter_regions(self) -> Iterable[MemoryRegion]: ...

    def iter_region_chunks(self, region: MemoryRegion, *, chunk_size: int = 0x20000) -> Iterator[tuple[int, bytes]]: ...

    def read(self, address: int, size: int) -> bytes: ...

    def write(self, address: int, data: bytes) -> None: ...


def _protect_to_string(value: int) -> str:
    if wintypes is None:  # pragma: no cover
        return "UNKNOWN"

    mapping = {
        0x01: "PAGE_NOACCESS",
        0x02: "PAGE_READONLY",
        0x04: "PAGE_READWRITE",
        0x08: "PAGE_WRITECOPY",
        0x10: "PAGE_EXECUTE",
        0x20: "PAGE_EXECUTE_READ",
        0x40: "PAGE_EXECUTE_READWRITE",
        0x80: "PAGE_EXECUTE_WRITECOPY",
        0x100: "PAGE_GUARD",
        0x200: "PAGE_NOCACHE",
        0x400: "PAGE_WRITECOMBINE",
    }
    protections = []
    for mask, name in mapping.items():
        if value & mask:
            protections.append(name)
    return "|".join(protections) if protections else "PAGE_UNKNOWN"


class WindowsMemoryAccessor:
    """Concrete accessor backed by kernel32 APIs."""

    PROCESS_QUERY_INFORMATION = 0x0400
    PROCESS_VM_OPERATION = 0x0008
    PROCESS_VM_READ = 0x0010
    PROCESS_VM_WRITE = 0x0020

    MEM_COMMIT = 0x1000
    MEM_FREE = 0x10000
    MEM_RESERVE = 0x2000
    PAGE_GUARD = 0x100
    PAGE_NOACCESS = 0x01

    def __init__(self, pid: int, *, handle: Optional[int] = None, driver: Optional["KernelDriverBridge"] = None):
        if wintypes is None:  # pragma: no cover - caught in tests
            raise RuntimeError("Windows APIs are not available on this platform")

        self.pid = pid
        self._kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        self._handle = handle or self._open_process(pid)
        self._lock = threading.RLock()
        self._driver = driver
        self._configure_prototypes()

    # Windows API bootstrapping -------------------------------------------------

    def _configure_prototypes(self) -> None:
        k32 = self._kernel32
        k32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        k32.OpenProcess.restype = wintypes.HANDLE

        class MEMORY_BASIC_INFORMATION(ctypes.Structure):
            _fields_ = [
                ("BaseAddress", ctypes.c_void_p),
                ("AllocationBase", ctypes.c_void_p),
                ("AllocationProtect", wintypes.DWORD),
                ("RegionSize", ctypes.c_size_t),
                ("State", wintypes.DWORD),
                ("Protect", wintypes.DWORD),
                ("Type", wintypes.DWORD),
            ]

        self._MEMORY_BASIC_INFORMATION = MEMORY_BASIC_INFORMATION
        k32.VirtualQueryEx.argtypes = [
            wintypes.HANDLE,
            wintypes.LPCVOID,
            ctypes.POINTER(MEMORY_BASIC_INFORMATION),
            ctypes.c_size_t,
        ]
        k32.VirtualQueryEx.restype = ctypes.c_size_t

        k32.ReadProcessMemory.argtypes = [
            wintypes.HANDLE,
            wintypes.LPCVOID,
            wintypes.LPVOID,
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_size_t),
        ]
        k32.ReadProcessMemory.restype = wintypes.BOOL

        k32.WriteProcessMemory.argtypes = [
            wintypes.HANDLE,
            wintypes.LPVOID,
            wintypes.LPCVOID,
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_size_t),
        ]
        k32.WriteProcessMemory.restype = wintypes.BOOL

        k32.CloseHandle.argtypes = [wintypes.HANDLE]
        k32.CloseHandle.restype = wintypes.BOOL

    def _open_process(self, pid: int) -> int:
        access = (
            self.PROCESS_QUERY_INFORMATION
            | self.PROCESS_VM_OPERATION
            | self.PROCESS_VM_READ
            | self.PROCESS_VM_WRITE
        )
        handle = self._kernel32.OpenProcess(access, False, pid)
        if not handle:
            raise OSError(ctypes.get_last_error(), f"OpenProcess failed for pid {pid}")
        return handle

    # MemoryAccessor interface --------------------------------------------------

    def close(self) -> None:
        with self._lock:
            if getattr(self, "_handle", None):
                self._kernel32.CloseHandle(self._handle)
                self._handle = None

    def iter_regions(self) -> Iterator[MemoryRegion]:
        mbi = self._MEMORY_BASIC_INFORMATION()
        address = 0
        while True:
            result = self._kernel32.VirtualQueryEx(
                self._handle,
                ctypes.c_void_p(address),
                ctypes.byref(mbi),
                ctypes.sizeof(mbi),
            )
            if not result:
                break

            base_address = int(mbi.BaseAddress or 0)
            region_size = int(mbi.RegionSize)
            state = int(mbi.State)
            protect = int(mbi.Protect)

            if state == self.MEM_COMMIT and protect not in (self.PAGE_GUARD, self.PAGE_NOACCESS):
                yield MemoryRegion(
                    base_address=base_address,
                    size=region_size,
                    protection=_protect_to_string(protect),
                )

            next_address = base_address + region_size
            if next_address <= address:
                break
            address = next_address

    def iter_region_chunks(self, region: MemoryRegion, *, chunk_size: int = 0x20000) -> Iterator[tuple[int, bytes]]:
        remaining = region.size
        address = region.base_address
        while remaining > 0:
            take = min(chunk_size, remaining)
            try:
                data = self.read(address, take)
            except ReadError:
                break
            if not data:
                break
            yield address, data
            actual = len(data)
            address += actual
            remaining -= actual
            if actual < take:
                break

    def read(self, address: int, size: int) -> bytes:
        if self._driver:
            try:
                data = self._driver.read(self.pid, address, size)
                if len(data) == size:
                    return data
                buffer = bytearray(data)
                offset = len(buffer)
            except OSError as exc:
                LOGGER.debug("Driver read failed, falling back: %s", exc)
                buffer = bytearray()
                offset = 0
        else:
            buffer = bytearray()
            offset = 0
        while offset < size:
            request = min(size - offset, 0x4000)
            chunk_buffer = (ctypes.c_ubyte * request)()
            read = ctypes.c_size_t(0)
            ok = self._kernel32.ReadProcessMemory(
                self._handle,
                ctypes.c_void_p(address + offset),
                chunk_buffer,
                request,
                ctypes.byref(read),
            )
            if not ok:
                if not buffer:
                    raise ReadError(ctypes.get_last_error())
                break
            if read.value == 0:
                break
            buffer.extend(chunk_buffer[: read.value])
            offset += read.value
        return bytes(buffer)

    def write(self, address: int, data: bytes) -> None:
        if self._driver:
            try:
                self._driver.write(self.pid, address, data)
                return
            except OSError as exc:
                LOGGER.debug("Driver write failed, falling back: %s", exc)
        written = ctypes.c_size_t(0)
        buffer = ctypes.create_string_buffer(data)
        ok = self._kernel32.WriteProcessMemory(
            self._handle,
            ctypes.c_void_p(address),
            buffer,
            len(data),
            ctypes.byref(written),
        )
        if not ok or written.value != len(data):
            raise WriteError(ctypes.get_last_error())

    # Context management --------------------------------------------------------

    def __enter__(self) -> "WindowsMemoryAccessor":
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()


class LocalMemoryAccessor:
    """In-memory accessor used for deterministic testing."""

    def __init__(self, backing: bytearray):
        self._backing = backing
        self.pid = 0
        self._lock = threading.RLock()
        self._region = MemoryRegion(base_address=0, size=len(backing), protection="LOCAL")

    def close(self) -> None:
        pass

    def iter_regions(self) -> Iterator[MemoryRegion]:
        yield self._region

    def iter_region_chunks(self, region: MemoryRegion, *, chunk_size: int = 0x20000) -> Iterator[tuple[int, bytes]]:
        with self._lock:
            data = bytes(self._backing[region.base_address : region.base_address + region.size])
        start = region.base_address
        for offset in range(0, len(data), chunk_size):
            chunk = data[offset : offset + chunk_size]
            yield start + offset, chunk

    def read(self, address: int, size: int) -> bytes:
        with self._lock:
            return bytes(self._backing[address : address + size])

    def write(self, address: int, data: bytes) -> None:
        with self._lock:
            self._backing[address : address + len(data)] = data


__all__ = [
    "MemoryRegion",
    "MemoryAccessor",
    "ReadError",
    "WriteError",
    "WindowsMemoryAccessor",
    "LocalMemoryAccessor",
]
LOGGER = logging.getLogger(__name__)

if TYPE_CHECKING:
    from .driver_bridge import KernelDriverBridge
