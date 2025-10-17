
#include <ntddk.h>

#include "shared/DriverIoctl.h"
#include "kernel/DriverOperations.h"

DRIVER_UNLOAD TshUnload;
DRIVER_DISPATCH TshDispatchCreateClose;
DRIVER_DISPATCH TshDispatchDeviceControl;

static VOID CompleteIrp(PIRP irp, NTSTATUS status, ULONG_PTR information)
{
    irp->IoStatus.Status = status;
    irp->IoStatus.Information = information;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driverObject, PUNICODE_STRING registryPath)
{
    UNREFERENCED_PARAMETER(registryPath);

    UNICODE_STRING deviceName = RTL_CONSTANT_STRING(TSH_DEVICE_NAME);
    UNICODE_STRING symLink = RTL_CONSTANT_STRING(TSH_DEVICE_SYM_LINK);
    PDEVICE_OBJECT deviceObject = NULL;

    NTSTATUS status = IoCreateDevice(
        driverObject,
        0,
        &deviceName,
        FILE_DEVICE_TSH,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = IoCreateSymbolicLink(&symLink, &deviceName);
    if (!NT_SUCCESS(status))
    {
        IoDeleteDevice(deviceObject);
        return status;
    }

    driverObject->DriverUnload = TshUnload;

    driverObject->MajorFunction[IRP_MJ_CREATE] = TshDispatchCreateClose;
    driverObject->MajorFunction[IRP_MJ_CLOSE] = TshDispatchCreateClose;
    driverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = TshDispatchDeviceControl;

    deviceObject->Flags |= DO_BUFFERED_IO;
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    TshMemorySubsystemInitialize();

    return STATUS_SUCCESS;
}

_Dispatch_type_(IRP_MJ_CREATE)
_Dispatch_type_(IRP_MJ_CLOSE)
NTSTATUS TshDispatchCreateClose(PDEVICE_OBJECT deviceObject, PIRP irp)
{
    UNREFERENCED_PARAMETER(deviceObject);

    CompleteIrp(irp, STATUS_SUCCESS, 0);
    return STATUS_SUCCESS;
}

_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
NTSTATUS TshDispatchDeviceControl(PDEVICE_OBJECT deviceObject, PIRP irp)
{
    UNREFERENCED_PARAMETER(deviceObject);

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    ULONG_PTR information = 0;
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;

    switch (stack->Parameters.DeviceIoControl.IoControlCode)
    {
        case IOCTL_TSH_PING:
        {
            if (stack->Parameters.DeviceIoControl.InputBufferLength >= sizeof(TSH_PING_REQUEST) &&
                stack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(TSH_PING_RESPONSE))
            {
                const PTSH_PING_REQUEST request = (PTSH_PING_REQUEST)irp->AssociatedIrp.SystemBuffer;
                PTSH_PING_RESPONSE response = (PTSH_PING_RESPONSE)irp->AssociatedIrp.SystemBuffer;

                response->Value = request->Token ^ 0xA5A5A5A5;
                information = sizeof(TSH_PING_RESPONSE);
                status = STATUS_SUCCESS;
            }
            else
            {
                status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }
        case IOCTL_TSH_QUERY_REGIONS:
            status = TshHandleQueryRegions(irp, stack, &information);
            break;
        case IOCTL_TSH_PATTERN_SCAN:
            status = TshHandlePatternScan(irp, stack, &information);
            break;
        case IOCTL_TSH_POINTER_TRACE:
            status = TshHandlePointerTrace(irp, stack, &information);
            break;
        case IOCTL_TSH_READ_MEMORY:
            status = TshHandleReadMemory(irp, stack, &information);
            break;
        case IOCTL_TSH_MONITOR_CONTROL:
            status = TshHandleMonitorRequest(irp, stack, &information);
            break;
        case IOCTL_TSH_PATCH_REQUEST:
            status = TshHandlePatchRequest(irp, stack, &information);
            break;
        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

    CompleteIrp(irp, status, information);
    return status;
}

VOID TshUnload(PDRIVER_OBJECT driverObject)
{
    UNICODE_STRING symLink = RTL_CONSTANT_STRING(TSH_DEVICE_SYM_LINK);
    IoDeleteSymbolicLink(&symLink);

    if (driverObject->DeviceObject != NULL)
    {
        IoDeleteDevice(driverObject->DeviceObject);
    }

    TshMemorySubsystemShutdown();
}
