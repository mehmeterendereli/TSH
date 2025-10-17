#include <ntifs.h>

#define _KERNEL_MODE
#include "shared/DriverIoctl.h"
#include "kernel/DriverOperations.h"
#undef _KERNEL_MODE

static NTSTATUS TshOpenProcessForQuery(
    _In_ ULONG processId,
    _Outptr_ PEPROCESS* processObject,
    _Out_ PHANDLE processHandle)
{
    if (!processObject || !processHandle)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *processObject = NULL;
    *processHandle = NULL;

    PEPROCESS targetProcess = NULL;
    NTSTATUS status = PsLookupProcessByProcessId(ULongToHandle(processId), &targetProcess);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    HANDLE handle = NULL;
    status = ObOpenObjectByPointer(
        targetProcess,
        OBJ_KERNEL_HANDLE,
        NULL,
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
        *PsProcessType,
        KernelMode,
        &handle);

    if (!NT_SUCCESS(status))
    {
        ObDereferenceObject(targetProcess);
        return status;
    }

    *processObject = targetProcess;
    *processHandle = handle;
    return STATUS_SUCCESS;
}

static BOOLEAN TshCallerHasDebugPrivilege()
{
    LUID luid = { 0 };
    luid.LowPart = SE_DEBUG_PRIVILEGE;
    return SeSinglePrivilegeCheck(luid, ExGetPreviousMode());
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

    HANDLE requestorPid = IoGetRequestorProcessId(irp);
    if (request.ProcessId != HandleToULong(requestorPid) && !TshCallerHasDebugPrivilege())
    {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL, "TSH: region query denied for pid %lu (requestor %lu)\n", request.ProcessId, HandleToULong(requestorPid));
        return STATUS_ACCESS_DENIED;
    }

    PEPROCESS targetProcess = NULL;
    HANDLE processHandle = NULL;
    NTSTATUS status = TshOpenProcessForQuery(request.ProcessId, &targetProcess, &processHandle);
    if (!NT_SUCCESS(status))
    {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "TSH: PsLookup/ObOpen failed for pid %lu (status 0x%08X)\n", request.ProcessId, status);
        return status;
    }

    PTSH_MEMORY_REGION regionBuffer = (PTSH_MEMORY_REGION)irp->AssociatedIrp.SystemBuffer;
    const ULONG outputLength = stack->Parameters.DeviceIoControl.OutputBufferLength;
    const ULONG capacity = (outputLength >= sizeof(TSH_MEMORY_REGION))
        ? (outputLength / sizeof(TSH_MEMORY_REGION))
        : 0;

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
        if (nextAddress == NULL || nextAddress <= (PUCHAR)baseAddress)
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

    if (processHandle != NULL)
    {
        ZwClose(processHandle);
    }

    if (targetProcess != NULL)
    {
        ObDereferenceObject(targetProcess);
    }

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    if (written < regionCount)
    {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL, "TSH: region query truncated for pid %lu (reported %lu of %lu)\n", request.ProcessId, written, regionCount);
        return STATUS_BUFFER_OVERFLOW;
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, "TSH: region query complete for pid %lu (regions %lu)\n", request.ProcessId, regionCount);
    return STATUS_SUCCESS;
}
