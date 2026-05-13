/*++ HID_XFER_PACKET extraction for UMDF (vhidmini2 util.c). --*/

#include "driver.h"
#include "util.tmh"

NTSTATUS
RequestGetHidXferPacket_ToReadFromDevice(
    _In_ WDFREQUEST Request,
    _Out_ HID_XFER_PACKET *Packet
    )
{
    NTSTATUS status;
    WDFMEMORY inputMemory;
    WDFMEMORY outputMemory;
    size_t inputBufferLength;
    size_t outputBufferLength;
    PVOID inputBuffer;
    PVOID outputBuffer;

    status = WdfRequestRetrieveInputMemory(Request, &inputMemory);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_HID, "RetrieveInputMemory read xfer %!STATUS!", status);
        return status;
    }
    inputBuffer = WdfMemoryGetBuffer(inputMemory, &inputBufferLength);
    if (inputBufferLength < sizeof(UCHAR)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_HID, "read xfer input too small %Iu", inputBufferLength);
        return STATUS_INVALID_BUFFER_SIZE;
    }
    Packet->reportId = *(PUCHAR)inputBuffer;

    status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_HID, "RetrieveOutputMemory read xfer %!STATUS!", status);
        return status;
    }
    outputBuffer = WdfMemoryGetBuffer(outputMemory, &outputBufferLength);
    Packet->reportBuffer = (PUCHAR)outputBuffer;
    Packet->reportBufferLen = (ULONG)outputBufferLength;
    return status;
}

NTSTATUS
RequestGetHidXferPacket_ToWriteToDevice(
    _In_ WDFREQUEST Request,
    _Out_ HID_XFER_PACKET *Packet
    )
{
    NTSTATUS status;
    WDFMEMORY inputMemory;
    WDFMEMORY outputMemory;
    size_t inputBufferLength;
    size_t outputBufferLength;
    PVOID inputBuffer;

    status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_HID, "RetrieveOutputMemory write xfer %!STATUS!", status);
        return status;
    }
    WdfMemoryGetBuffer(outputMemory, &outputBufferLength);
    Packet->reportId = (UCHAR)outputBufferLength;

    status = WdfRequestRetrieveInputMemory(Request, &inputMemory);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_HID, "RetrieveInputMemory write xfer %!STATUS!", status);
        return status;
    }
    inputBuffer = WdfMemoryGetBuffer(inputMemory, &inputBufferLength);
    Packet->reportBuffer = (PUCHAR)inputBuffer;
    Packet->reportBufferLen = (ULONG)inputBufferLength;
    return status;
}
