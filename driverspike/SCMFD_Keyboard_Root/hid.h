/*++ HID minidriver IOCTL handling (vhidmini2 UMDF2 parity). --*/

#ifndef HID_H
#define HID_H

#include <wdf.h>
#include <hidport.h>
#include "driverspike-common.h"
#include "queue.h"

EXTERN_C_START

#define HIDMINI_PID     0xFEED
#define HIDMINI_VID     0xDEED
#define HIDMINI_VERSION 0x0101

#define CONTROL_FEATURE_REPORT_ID CONTROL_COLLECTION_REPORT_ID

extern HID_DESCRIPTOR G_DefaultHidDescriptor;
extern HID_REPORT_DESCRIPTOR G_DefaultReportDescriptor[];

NTSTATUS RequestCopyFromBuffer(
    _In_ WDFREQUEST Request,
    _In_ PVOID SourceBuffer,
    _In_ size_t NumBytesToCopyFrom
    );

NTSTATUS CompleteHidReadRequest(
    _In_ WDFREQUEST Request,
    _In_ PVOID SourceBuffer,
    _In_ size_t NumBytes
    );

NTSTATUS RequestGetHidXferPacket_ToReadFromDevice(
    _In_ WDFREQUEST Request,
    _Out_ HID_XFER_PACKET *Packet
    );

NTSTATUS RequestGetHidXferPacket_ToWriteToDevice(
    _In_ WDFREQUEST Request,
    _Out_ HID_XFER_PACKET *Packet
    );

VOID SCMFD_Keyboard_RootEvtIoHidDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    );

NTSTATUS ReadReport(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST Request,
    _Always_(_Out_) BOOLEAN *CompleteRequest
    );

NTSTATUS WriteReport(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST Request
    );

NTSTATUS GetFeature(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST Request
    );

NTSTATUS SetFeature(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST Request
    );

NTSTATUS GetInputReport(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST Request,
    _Always_(_Out_) BOOLEAN *CompleteRequest
    );

NTSTATUS SetOutputReport(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST Request
    );

NTSTATUS GetString(_In_ WDFREQUEST Request);
NTSTATUS GetIndexedString(_In_ WDFREQUEST Request);

EXTERN_C_END

#endif
