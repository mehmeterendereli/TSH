#include "MemoryPatcherIoctl.h"

static NTSTATUS MemoryPatcherCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
static NTSTATUS MemoryPatcherDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
static VOID MemoryPatcherUnload(_In_ PDRIVER_OBJECT DriverObject);

DRIVER_INITIALIZE DriverEntry;

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status;
    PDEVICE_OBJECT deviceObject = NULL;
    UNICODE_STRING deviceName = RTL_CONSTANT_STRING(MEMPATCH_DEVICE_NAME);
    UNICODE_STRING dosName = RTL_CONSTANT_STRING(MEMPATCH_DOS_DEVICE);

    status = IoCreateDevice(
        DriverObject,
        0,
        &deviceName,
        FILE_DEVICE_MEMPATCH,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = IoCreateSymbolicLink(&dosName, &deviceName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(deviceObject);
        return status;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE] = MemoryPatcherCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = MemoryPatcherCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MemoryPatcherDeviceControl;
    DriverObject->DriverUnload = MemoryPatcherUnload;

    deviceObject->Flags |= DO_BUFFERED_IO;
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}

static VOID
MemoryPatcherUnload(
    _In_ PDRIVER_OBJECT DriverObject
    )
{
    UNICODE_STRING dosName = RTL_CONSTANT_STRING(MEMPATCH_DOS_DEVICE);
    IoDeleteSymbolicLink(&dosName);
    if (DriverObject->DeviceObject) {
        IoDeleteDevice(DriverObject->DeviceObject);
    }
}

static NTSTATUS
MemoryPatcherCreateClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
    )
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static NTSTATUS
HandleReadRequest(
    _In_reads_bytes_(InputLength) PMEMPATCH_RW_REQUEST Request,
    _In_ ULONG InputLength,
    _Inout_updates_bytes_(OutputLength) PVOID SystemBuffer,
    _In_ ULONG OutputLength,
    _Out_ PULONG_PTR BytesTransferred
    )
{
    if (InputLength < sizeof(MEMPATCH_RW_REQUEST)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    MEMPATCH_RW_REQUEST command = *Request;
    if (OutputLength < command.Size) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    PEPROCESS targetProcess = NULL;
    NTSTATUS status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)command.ProcessId, &targetProcess);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    SIZE_T copied = 0;
    status = MmCopyVirtualMemory(
        targetProcess,
        (PVOID)(ULONG_PTR)command.Address,
        PsGetCurrentProcess(),
        SystemBuffer,
        command.Size,
        KernelMode,
        &copied);

    ObDereferenceObject(targetProcess);

    if (NT_SUCCESS(status)) {
        *BytesTransferred = copied;
    }

    return status;
}

static NTSTATUS
HandleWriteRequest(
    _In_reads_bytes_(InputLength) PMEMPATCH_RW_REQUEST Request,
    _In_ ULONG InputLength,
    _In_reads_bytes_(InputLength) PVOID SystemBuffer,
    _Out_ PULONG_PTR BytesTransferred
    )
{
    if (InputLength < sizeof(MEMPATCH_RW_REQUEST)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    MEMPATCH_RW_REQUEST command = *Request;
    if (InputLength < sizeof(MEMPATCH_RW_REQUEST) + command.Size) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    PUCHAR payload = ((PUCHAR)SystemBuffer) + sizeof(MEMPATCH_RW_REQUEST);

    PEPROCESS targetProcess = NULL;
    NTSTATUS status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)command.ProcessId, &targetProcess);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    SIZE_T copied = 0;
    status = MmCopyVirtualMemory(
        PsGetCurrentProcess(),
        payload,
        targetProcess,
        (PVOID)(ULONG_PTR)command.Address,
        command.Size,
        KernelMode,
        &copied);

    ObDereferenceObject(targetProcess);

    if (NT_SUCCESS(status)) {
        *BytesTransferred = copied;
    }

    return status;
}

static NTSTATUS
MemoryPatcherDeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
    )
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    PVOID systemBuffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG inputLength = stack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outputLength = stack->Parameters.DeviceIoControl.OutputBufferLength;
    ULONG_PTR bytesTransferred = 0;
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;

    switch (code) {
    case IOCTL_MEMPATCH_READ:
        status = HandleReadRequest((PMEMPATCH_RW_REQUEST)systemBuffer, inputLength, systemBuffer, outputLength, &bytesTransferred);
        break;
    case IOCTL_MEMPATCH_WRITE:
        status = HandleWriteRequest((PMEMPATCH_RW_REQUEST)systemBuffer, inputLength, systemBuffer, &bytesTransferred);
        break;
    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesTransferred;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}
