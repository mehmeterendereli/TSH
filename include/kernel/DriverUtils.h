#pragma once

#include <ntifs.h>

#ifdef __cplusplus
extern "C" {
#endif

NTSTATUS TshOpenTargetProcess(
    _In_ ULONG processId,
    _In_ ACCESS_MASK desiredAccess,
    _Outptr_ PEPROCESS* processObject,
    _Out_ HANDLE* processHandle);

VOID TshCloseTargetProcess(
    _In_opt_ PEPROCESS processObject,
    _In_opt_ HANDLE processHandle);

BOOLEAN TshCallerOwnsProcess(
    _In_ PIRP irp,
    _In_ ULONG targetProcessId);

BOOLEAN TshCallerHasDebugPrivilege(VOID);

NTSTATUS TshCopyFromProcess(
    _In_ PEPROCESS processObject,
    _In_ PVOID sourceAddress,
    _Out_writes_bytes_(size) PVOID destinationBuffer,
    _In_ SIZE_T size);

NTSTATUS TshCopyToProcess(
    _In_ PEPROCESS processObject,
    _In_reads_bytes_(size) PVOID sourceBuffer,
    _In_ PVOID destinationAddress,
    _In_ SIZE_T size);

#ifdef __cplusplus
}
#endif

