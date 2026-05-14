/*++ Default queue — UMDF vhidmini2 uses EvtIoDeviceControl (mshidumdf path). --*/

#include "driver.h"
#include "queue.tmh"

NTSTATUS
SCMFD_Joystick_RootQueueInitialize(
    _In_ WDFDEVICE Device
    )
{
    WDFQUEUE queue;
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDF_OBJECT_ATTRIBUTES queueAttributes;
    PQUEUE_CONTEXT queueContext;

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);

    queueConfig.EvtIoDeviceControl = SCMFD_Joystick_RootEvtIoDeviceControl;
    queueConfig.EvtIoStop = SCMFD_Joystick_RootEvtIoStop;

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&queueAttributes, QUEUE_CONTEXT);

    status = WdfIoQueueCreate(Device, &queueConfig, &queueAttributes, &queue);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfIoQueueCreate %!STATUS!", status);
        return status;
    }

    queueContext = QueueGetContext(queue);
    queueContext->Queue = queue;
    queueContext->DeviceContext = DeviceGetContext(Device);
    queueContext->OutputReport = 0;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "Default queue configured (EvtIoDeviceControl=HID path)");
    return STATUS_SUCCESS;
}

VOID
SCMFD_Joystick_RootEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
{
    SCMFD_Joystick_RootEvtIoHidDeviceControl(Queue, Request, OutputBufferLength, InputBufferLength, IoControlCode);
}

VOID
SCMFD_Joystick_RootEvtIoStop(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG ActionFlags
    )
{
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(ActionFlags);
}
