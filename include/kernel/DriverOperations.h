#pragma once

#include <ntddk.h>

#include "shared/Protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

NTSTATUS TshHandleQueryRegions(PIRP irp, PIO_STACK_LOCATION stack, PULONG_PTR information);
NTSTATUS TshHandlePatternScan(PIRP irp, PIO_STACK_LOCATION stack, PULONG_PTR information);
NTSTATUS TshHandlePointerTrace(PIRP irp, PIO_STACK_LOCATION stack, PULONG_PTR information);
NTSTATUS TshHandleMonitorRequest(PIRP irp, PIO_STACK_LOCATION stack, PULONG_PTR information);
NTSTATUS TshHandlePatchRequest(PIRP irp, PIO_STACK_LOCATION stack, PULONG_PTR information);

VOID TshMemorySubsystemInitialize();
VOID TshMemorySubsystemShutdown();

#ifdef __cplusplus
}
#endif

