#include <ntddk.h>

#define _KERNEL_MODE
#include "shared/DriverIoctl.h"
#include "kernel/DriverOperations.h"
#undef _KERNEL_MODE

NTSTATUS TshHandleMonitorRequest(PIRP irp, PIO_STACK_LOCATION stack, PULONG_PTR information)
{
    UNREFERENCED_PARAMETER(irp);
    UNREFERENCED_PARAMETER(stack);

    if (information)
    {
        *information = 0;
    }

    return STATUS_NOT_IMPLEMENTED;
}

