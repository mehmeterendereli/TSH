#include <ntifs.h>


#include "kernel/DriverUtils.h"

static NTSTATUS TshLookupProcessById(_In_ ULONG processId, _Outptr_ PEPROCESS* processObject)
{
    if (!processObject)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *processObject = NULL;
    return PsLookupProcessByProcessId(ULongToHandle(processId), processObject);
}

NTSTATUS TshOpenTargetProcess(
    _In_ ULONG processId,
    _In_ ACCESS_MASK desiredAccess,
    _Outptr_ PEPROCESS* processObject,
    _Out_ HANDLE* processHandle)
{
    if (!processObject || !processHandle)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *processObject = NULL;
    *processHandle = NULL;

    PEPROCESS target = NULL;
    NTSTATUS status = TshLookupProcessById(processId, &target);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    HANDLE handle = NULL;
    status = ObOpenObjectByPointer(
        target,
        OBJ_KERNEL_HANDLE,
        NULL,
        desiredAccess,
        *PsProcessType,
        KernelMode,
        &handle);

    if (!NT_SUCCESS(status))
    {
        ObDereferenceObject(target);
        return status;
    }

    *processObject = target;
    *processHandle = handle;
    return STATUS_SUCCESS;
}

VOID TshCloseTargetProcess(
    _In_opt_ PEPROCESS processObject,
    _In_opt_ HANDLE processHandle)
{
    if (processHandle != NULL)
    {
        ZwClose(processHandle);
    }

    if (processObject != NULL)
    {
        ObDereferenceObject(processObject);
    }
}

BOOLEAN TshCallerOwnsProcess(
    _In_ PIRP irp,
    _In_ ULONG targetProcessId)
{
    const ULONG callerPid = (ULONG)(ULONG_PTR)IoGetRequestorProcessId(irp);
    return callerPid == targetProcessId;
}

BOOLEAN TshCallerHasDebugPrivilege(VOID)
{
    LUID luid = { 0 };
    luid.LowPart = SE_DEBUG_PRIVILEGE;
    return SeSinglePrivilegeCheck(luid, ExGetPreviousMode());
}

NTSTATUS TshCopyFromProcess(
    _In_ PEPROCESS processObject,
    _In_ PVOID sourceAddress,
    _Out_writes_bytes_(size) PVOID destinationBuffer,
    _In_ SIZE_T size)
{
    if (!processObject || !sourceAddress || !destinationBuffer || size == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    SIZE_T transferred = 0;
    NTSTATUS status = MmCopyVirtualMemory(
        processObject,
        sourceAddress,
        PsGetCurrentProcess(),
        destinationBuffer,
        size,
        KernelMode,
        &transferred);

    if (status == STATUS_SUCCESS && transferred != size)
    {
        status = STATUS_PARTIAL_COPY;
    }

    return status;
}

NTSTATUS TshCopyToProcess(
    _In_ PEPROCESS processObject,
    _In_reads_bytes_(size) PVOID sourceBuffer,
    _In_ PVOID destinationAddress,
    _In_ SIZE_T size)
{
    if (!processObject || !sourceBuffer || !destinationAddress || size == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    SIZE_T transferred = 0;
    NTSTATUS status = MmCopyVirtualMemory(
        PsGetCurrentProcess(),
        sourceBuffer,
        processObject,
        destinationAddress,
        size,
        KernelMode,
        &transferred);

    if (status == STATUS_SUCCESS && transferred != size)
    {
        status = STATUS_PARTIAL_COPY;
    }

    return status;
}

