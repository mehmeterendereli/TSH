#pragma once

#include <ntifs.h>
#ifndef PROCESS_QUERY_INFORMATION
#define PROCESS_QUERY_INFORMATION            0x0400
#endif
#ifndef PROCESS_VM_READ
#define PROCESS_VM_READ                      0x0010
#endif
#ifndef PROCESS_VM_WRITE
#define PROCESS_VM_WRITE                     0x0020
#endif
#ifndef TSH_MIN
#define TSH_MIN(a,b) (( (a) < (b) ) ? (a) : (b))
#endif

#ifndef PROCESS_VM_OPERATION
#define PROCESS_VM_OPERATION                 0x0008
#endif
#ifndef PROCESS_QUERY_LIMITED_INFORMATION
#define PROCESS_QUERY_LIMITED_INFORMATION    0x1000
#endif


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

