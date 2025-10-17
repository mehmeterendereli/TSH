#include <ntddk.h>

#include "shared/DriverIoctl.h"
#include "kernel/DriverOperations.h"

NTSTATUS TshHandlePatternScan(PIRP irp, PIO_STACK_LOCATION stack, PULONG_PTR information)
{
    UNREFERENCED_PARAMETER(irp);
    UNREFERENCED_PARAMETER(stack);

    if (information)
    {
        *information = 0;
    }

    return STATUS_NOT_IMPLEMENTED;
}

