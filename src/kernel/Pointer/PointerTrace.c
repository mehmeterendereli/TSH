#include <ntifs.h>

#include "shared/DriverIoctl.h"
#include "kernel/DriverOperations.h"
#include "kernel/DriverUtils.h"

NTSTATUS TshHandlePointerTrace(PIRP irp, PIO_STACK_LOCATION stack, PULONG_PTR information)
{
    if (!irp || !stack)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (information)
    {
        *information = 0;
    }

    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(TSH_POINTER_TRACE_REQUEST))
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    const TSH_POINTER_TRACE_REQUEST* request = (const TSH_POINTER_TRACE_REQUEST*)irp->AssociatedIrp.SystemBuffer;
    const ULONG processId = request->ProcessId;

    if (!TshCallerOwnsProcess(irp, processId) && !TshCallerHasDebugPrivilege())
    {
        return STATUS_ACCESS_DENIED;
    }

    const SIZE_T inputRemainder = stack->Parameters.DeviceIoControl.InputBufferLength - sizeof(TSH_POINTER_TRACE_REQUEST);
    const SIZE_T offsetCount = inputRemainder / sizeof(LONGLONG);
    const ULONG requestedDepth = (request->MaxDepth == 0)
        ? (ULONG)offsetCount
        : TSH_MIN(request->MaxDepth, (ULONG)offsetCount);

    if (requestedDepth == 0)
    {
        return STATUS_SUCCESS;
    }

    const SIZE_T outputLength = stack->Parameters.DeviceIoControl.OutputBufferLength;
    const SIZE_T capacity = outputLength / sizeof(TSH_POINTER_TRACE_NODE);
    if (capacity == 0)
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    PEPROCESS processObject = NULL;
    HANDLE processHandle = NULL;
    NTSTATUS status = TshOpenTargetProcess(processId, PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION, &processObject, &processHandle);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    const LONGLONG* offsets = (const LONGLONG*)(request + 1);
    PTSH_POINTER_TRACE_NODE nodes = (PTSH_POINTER_TRACE_NODE)irp->AssociatedIrp.SystemBuffer;

    SIZE_T written = 0;
    ULONGLONG currentAddress = request->BaseAddress;

    for (ULONG depth = 0; depth < requestedDepth && written < capacity; ++depth)
    {
        ULONGLONG value = 0;
        nodes[written].Address = currentAddress;

        status = TshCopyFromProcess(processObject, (PVOID)(ULONG_PTR)currentAddress, &value, sizeof(value));
        if (!NT_SUCCESS(status))
        {
            nodes[written].Value = 0;
            written++;
            break;
        }

        nodes[written].Value = value;
        written++;

        const LONGLONG offset = offsets[depth];
        currentAddress = value + offset;
    }

    if (information)
    {
        *information = written * sizeof(TSH_POINTER_TRACE_NODE);
    }

    TshCloseTargetProcess(processObject, processHandle);

    return status;
}
