"""Kernel-mode driver bridge for privileged memory operations."""

from __future__ import annotations

import ctypes
import struct
from ctypes import wintypes
from typing import Optional

from .logging_config import configure_logging

LOGGER = configure_logging(__name__)

GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3
FILE_ATTRIBUTE_NORMAL = 0x80

FILE_DEVICE_MEMPATCH = 0x8000
METHOD_BUFFERED = 0
FILE_READ_ACCESS = 0x0001
FILE_WRITE_ACCESS = 0x0002


def _ctl_code(device: int, function: int, method: int, access: int) -> int:
    return (device << 16) | (access << 14) | (function << 2) | method


IOCTL_MEMPATCH_READ = _ctl_code(FILE_DEVICE_MEMPATCH, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)
IOCTL_MEMPATCH_WRITE = _ctl_code(FILE_DEVICE_MEMPATCH, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

_RW_STRUCT = struct.Struct("<IQQ")


class KernelDriverBridge:
    """Thin wrapper around the MemoryPatcher kernel driver."""

    def __init__(self, device_path: str = r"\\.\MemoryPatcher") -> None:
        self._device_path = device_path
        self._kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        self._set_prototypes()
        self._handle: Optional[int] = None

    def _set_prototypes(self) -> None:
        self._kernel32.CreateFileW.argtypes = [
            wintypes.LPCWSTR,
            wintypes.DWORD,
            wintypes.DWORD,
            wintypes.LPVOID,
            wintypes.DWORD,
            wintypes.DWORD,
            wintypes.HANDLE,
        ]
        self._kernel32.CreateFileW.restype = wintypes.HANDLE

        self._kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
        self._kernel32.CloseHandle.restype = wintypes.BOOL

        self._kernel32.DeviceIoControl.argtypes = [
            wintypes.HANDLE,
            wintypes.DWORD,
            wintypes.LPVOID,
            wintypes.DWORD,
            wintypes.LPVOID,
            wintypes.DWORD,
            ctypes.POINTER(wintypes.DWORD),
            wintypes.LPVOID,
        ]
        self._kernel32.DeviceIoControl.restype = wintypes.BOOL

    def open(self) -> None:
        if self._handle:
            return
        handle = self._kernel32.CreateFileW(
            self._device_path,
            GENERIC_READ | GENERIC_WRITE,
            0,
            None,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            None,
        )
        if handle == wintypes.HANDLE(-1).value:
            error = ctypes.get_last_error()
            raise OSError(error, "Failed to open MemoryPatcher driver")
        self._handle = handle

    def close(self) -> None:
        if self._handle:
            self._kernel32.CloseHandle(self._handle)
            self._handle = None

    def __enter__(self) -> "KernelDriverBridge":
        self.open()
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()

    def _ensure_open(self) -> wintypes.HANDLE:
        if not self._handle:
            self.open()
        return wintypes.HANDLE(self._handle)

    def read(self, pid: int, address: int, size: int) -> bytes:
        handle = self._ensure_open()
        header = _RW_STRUCT.pack(pid, address, size)
        buffer_size = max(len(header), size)
        buffer = ctypes.create_string_buffer(buffer_size)
        ctypes.memmove(buffer, header, len(header))
        transferred = wintypes.DWORD(0)
        success = self._kernel32.DeviceIoControl(
            handle,
            IOCTL_MEMPATCH_READ,
            buffer,
            len(header),
            buffer,
            size,
            ctypes.byref(transferred),
            None,
        )
        if not success:
            error = ctypes.get_last_error()
            raise OSError(error, "Driver read failed")
        return buffer.raw[: transferred.value]

    def write(self, pid: int, address: int, data: bytes) -> None:
        handle = self._ensure_open()
        header = _RW_STRUCT.pack(pid, address, len(data))
        buffer = ctypes.create_string_buffer(len(header) + len(data))
        ctypes.memmove(buffer, header, len(header))
        if data:
            ctypes.memmove(ctypes.addressof(buffer) + len(header), data, len(data))
        transferred = wintypes.DWORD(0)
        success = self._kernel32.DeviceIoControl(
            handle,
            IOCTL_MEMPATCH_WRITE,
            buffer,
            len(buffer),
            None,
            0,
            ctypes.byref(transferred),
            None,
        )
        if not success or transferred.value != len(data):
            error = ctypes.get_last_error()
            raise OSError(error, "Driver write failed")


__all__ = ["KernelDriverBridge"]
