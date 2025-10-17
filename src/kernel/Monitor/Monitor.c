#include <ntifs.h>

#include "shared/DriverIoctl.h"
#include "kernel/DriverOperations.h"
#include "kernel/DriverUtils.h"

NTSTATUS TshHandleMonitorRequest(PIRP irp, PIO_STACK_LOCATION stack, PULONG_PTR information)
{
    if (!irp || !stack)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (information)
    {
        *information = 0;
    }

    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(TSH_MONITOR_REQUEST))
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    const TSH_MONITOR_REQUEST* request = (const TSH_MONITOR_REQUEST*)irp->AssociatedIrp.SystemBuffer;
    const SIZE_T remaining = stack->Parameters.DeviceIoControl.InputBufferLength - sizeof(TSH_MONITOR_REQUEST);
    const SIZE_T availableEntries = remaining / sizeof(TSH_MONITOR_ENTRY);
    const ULONG requestedEntries = TSH_MIN(request->EntryCount, (ULONG)availableEntries);

    if (requestedEntries == 0)
    {
        return STATUS_SUCCESS;
    }

    const SIZE_T outputLength = stack->Parameters.DeviceIoControl.OutputBufferLength;
    const SIZE_T capacity = outputLength / sizeof(TSH_MONITOR_SAMPLE);
    if (capacity == 0)
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
    NTSTATUS status = TshOpenTargetProcess(processId, PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION, &processObject, &processHandle);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    const TSH_MONITOR_ENTRY* entries = (const TSH_MONITOR_ENTRY*)(request + 1);
    PTSH_MONITOR_SAMPLE samples = (PTSH_MONITOR_SAMPLE)irp->AssociatedIrp.SystemBuffer;

    const SIZE_T maxSamples = (SIZE_T)TSH_MIN(requestedEntries, (ULONG)capacity);
    RtlZeroMemory(samples, maxSamples * sizeof(TSH_MONITOR_SAMPLE));

    NTSTATUS finalStatus = STATUS_SUCCESS;

    for (SIZE_T index = 0; index < maxSamples; ++index)
    {
        const TSH_MONITOR_ENTRY* entry = &entries[index];
        PTSH_MONITOR_SAMPLE sample = &samples[index];

        sample->Address = entry->Address;
        sample->RequestedSize = entry->Size;
        sample->CapturedSize = 0;
        sample->Status = STATUS_INVALID_PARAMETER;

        const SIZE_T copySize = TSH_MIN((SIZE_T)entry->Size, (SIZE_T)TSH_MONITOR_SAMPLE_MAX_BYTES);
        if (copySize == 0)
        {
            continue;
        }

        status = TshCopyFromProcess(processObject, (PVOID)(ULONG_PTR)entry->Address, sample->Data, copySize);
        if (NT_SUCCESS(status))
        {
            sample->CapturedSize = (ULONG)copySize;
            sample->Status = STATUS_SUCCESS;
        }
        else
        {
            sample->Status = status;
            if (NT_SUCCESS(finalStatus))
            {
                finalStatus = status;
            }
        }
    }

    if (information)
    {
        *information = maxSamples * sizeof(TSH_MONITOR_SAMPLE);
    }

    TshCloseTargetProcess(processObject, processHandle);
    return finalStatus;
}
