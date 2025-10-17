#include <ntifs.h>

#define _KERNEL_MODE
#include "shared/DriverIoctl.h"
#include "kernel/DriverOperations.h"
#include "kernel/DriverUtils.h"
#undef _KERNEL_MODE

NTSTATUS TshHandlePatchRequest(PIRP irp, PIO_STACK_LOCATION stack, PULONG_PTR information)
{
    if (!irp || !stack)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (information)
    {
        *information = 0;
    }

    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(TSH_PATCH_REQUEST))
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    const TSH_PATCH_REQUEST* request = (const TSH_PATCH_REQUEST*)irp->AssociatedIrp.SystemBuffer;
    if (request->PayloadSize == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    const SIZE_T totalSize = sizeof(TSH_PATCH_REQUEST) + request->PayloadSize;
    if (stack->Parameters.DeviceIoControl.InputBufferLength < totalSize)
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    const ULONG processId = request->ProcessId;
    if (!TshCallerOwnsProcess(irp, processId) && !TshCallerHasDebugPrivilege())
    {
        return STATUS_ACCESS_DENIED;
    }

    PEPROCESS processObject = NULL;
    HANDLE processHandle = NULL;
    NTSTATUS status = TshOpenTargetProcess(
        processId,
        PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_LIMITED_INFORMATION,
        &processObject,
        &processHandle);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    const PVOID payload = (const UCHAR*)(request + 1);

    status = TshCopyToProcess(
        processObject,
        (PVOID)payload,
        (PVOID)(ULONG_PTR)request->TargetAddress,
        request->PayloadSize);

    TshCloseTargetProcess(processObject, processHandle);

    if (stack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(TSH_PATCH_RESPONSE))
    {
        PTSH_PATCH_RESPONSE response = (PTSH_PATCH_RESPONSE)irp->AssociatedIrp.SystemBuffer;
        response->BytesWritten = NT_SUCCESS(status) ? request->PayloadSize : 0;
        response->Status = status;
        if (information)
        {
            *information = sizeof(TSH_PATCH_RESPONSE);
        }
    }

    return status;
}
