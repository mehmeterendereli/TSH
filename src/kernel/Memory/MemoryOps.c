#include <ntifs.h>

#define _KERNEL_MODE
#include "shared/DriverIoctl.h"
#include "kernel/DriverOperations.h"
#include "kernel/DriverUtils.h"
#undef _KERNEL_MODE

static BOOLEAN TshHasAccessForProcess(PIRP irp, ULONG processId)
{
    return TshCallerOwnsProcess(irp, processId) || TshCallerHasDebugPrivilege();
}

VOID TshMemorySubsystemInitialize()
{
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, "TSH: memory subsystem initialized\n");
}

VOID TshMemorySubsystemShutdown()
{
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, "TSH: memory subsystem shutdown\n");
}

NTSTATUS TshHandleQueryRegions(PIRP irp, PIO_STACK_LOCATION stack, PULONG_PTR information)
{
    if (!irp || !stack)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(TSH_REGION_QUERY_REQUEST))
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    const TSH_REGION_QUERY_REQUEST request = *(const TSH_REGION_QUERY_REQUEST*)irp->AssociatedIrp.SystemBuffer;
    if (information)
    {
        *information = 0;
    }

    if (!TshHasAccessForProcess(irp, request.ProcessId))
    {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL, "TSH: region query denied for pid %lu\n", request.ProcessId);
        return STATUS_ACCESS_DENIED;
    }

    PEPROCESS processObject = NULL;
    HANDLE processHandle = NULL;
    NTSTATUS status = TshOpenTargetProcess(request.ProcessId, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, &processObject, &processHandle);
    if (!NT_SUCCESS(status))
    {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "TSH: failed to open pid %lu (status 0x%08X)\n", request.ProcessId, status);
        return status;
    }

    PTSH_MEMORY_REGION regionBuffer = (PTSH_MEMORY_REGION)irp->AssociatedIrp.SystemBuffer;
    const ULONG outputLength = stack->Parameters.DeviceIoControl.OutputBufferLength;
    const ULONG capacity = (outputLength >= sizeof(TSH_MEMORY_REGION)) ? (outputLength / sizeof(TSH_MEMORY_REGION)) : 0;

    ULONG regionCount = 0;
    ULONG written = 0;
    PVOID baseAddress = NULL;

    for (;;)
    {
        MEMORY_BASIC_INFORMATION mbi = { 0 };
        SIZE_T returnLength = 0;
        NTSTATUS queryStatus = ZwQueryVirtualMemory(
            processHandle,
            baseAddress,
            MemoryBasicInformation,
            &mbi,
            sizeof(mbi),
            &returnLength);

        if (!NT_SUCCESS(queryStatus))
        {
            if (queryStatus == STATUS_INVALID_PARAMETER || queryStatus == STATUS_ACCESS_DENIED)
            {
                status = STATUS_SUCCESS;
            }
            else
            {
                status = queryStatus;
            }
            break;
        }

        if (mbi.State == MEM_COMMIT)
        {
            regionCount++;
            if (written < capacity)
            {
                PTSH_MEMORY_REGION entry = &regionBuffer[written];
                entry->BaseAddress = (ULONG_PTR)mbi.BaseAddress;
                entry->Size = mbi.RegionSize;
                entry->Protection = mbi.Protect;
                entry->State = mbi.State;
                entry->Type = mbi.Type;
                entry->Reserved = 0;
                written++;
            }
        }

        const PUCHAR nextAddress = (PUCHAR)mbi.BaseAddress + mbi.RegionSize;
        if (!nextAddress || nextAddress <= (PUCHAR)baseAddress)
        {
            status = STATUS_SUCCESS;
            break;
        }

        baseAddress = (PVOID)nextAddress;
    }

    if (information)
    {
        *information = written * sizeof(TSH_MEMORY_REGION);
    }

    TshCloseTargetProcess(processObject, processHandle);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    if (written < regionCount)
    {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL, "TSH: region query truncated for pid %lu (reported %lu / %lu)\n", request.ProcessId, written, regionCount);
        return STATUS_BUFFER_OVERFLOW;
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, "TSH: region query complete for pid %lu (regions %lu)\n", request.ProcessId, regionCount);
    return STATUS_SUCCESS;
}

NTSTATUS TshHandleReadMemory(PIRP irp, PIO_STACK_LOCATION stack, PULONG_PTR information)
{
    if (!irp || !stack)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(TSH_READ_MEMORY_REQUEST))
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    const TSH_READ_MEMORY_REQUEST request = *(const TSH_READ_MEMORY_REQUEST*)irp->AssociatedIrp.SystemBuffer;
    if (information)
    {
        *information = 0;
    }

    if (request.Size == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (stack->Parameters.DeviceIoControl.OutputBufferLength < request.Size)
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (!TshHasAccessForProcess(irp, request.ProcessId))
    {
        return STATUS_ACCESS_DENIED;
    }

    PEPROCESS processObject = NULL;
    HANDLE processHandle = NULL;
    NTSTATUS status = TshOpenTargetProcess(request.ProcessId, PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, &processObject, &processHandle);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    UNREFERENCED_PARAMETER(processHandle);

    status = TshCopyFromProcess(
        processObject,
        (PVOID)(ULONG_PTR)request.SourceAddress,
        irp->AssociatedIrp.SystemBuffer,
        request.Size);

    TshCloseTargetProcess(processObject, processHandle);

    if (NT_SUCCESS(status) && information)
    {
        *information = request.Size;
    }

    return status;
}

