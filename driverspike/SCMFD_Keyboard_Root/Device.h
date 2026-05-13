/*++ Device context (vhidmini2-aligned). --*/

#ifndef DEVICE_H
#define DEVICE_H

#include <wdf.h>
#include <hidport.h>
#include "public.h"
#include "../driverspike-common.h"

EXTERN_C_START

typedef UCHAR HID_REPORT_DESCRIPTOR, *PHID_REPORT_DESCRIPTOR;

typedef struct _DEVICE_CONTEXT {
    WDFDEVICE Device;
    WDFQUEUE DefaultQueue;
    WDFQUEUE ManualQueue;
    HID_DEVICE_ATTRIBUTES HidDeviceAttributes;
    BYTE DeviceData;
    HID_DESCRIPTOR HidDescriptor;
    PHID_REPORT_DESCRIPTOR ReportDescriptor;
    BOOLEAN ReadReportDescFromRegistry;
    // Temporary timer-driven sign-of-life state. Later tickets can keep
    // command lifecycle state separate from this bring-up-only fallback.
    ULONG DebugInstanceOrdinal;
    WDFREQUEST LastForwardedReadRequest;
    ULONG LastForwardedReadIoctl;
    ULONG Sequence;
    ULONG KeyDownCount;
    ULONG KeyUpCount;
    ULONG ReleaseAllCount;
    ULONG AutoReleaseCount;
    ULONG DuplicateNoOpCount;
    ULONG RolloverRejectCount;
    ULONG TimerTickCount;
    ULONG ReportsCompletedCount;
    ULONG EmptyTickCount;
    NTSTATUS LastCompletionStatus;
    UCHAR LastCompletedReport[INPUT_REPORT_SIZE_CB];
    UCHAR CurrentModifier;
    UCHAR CurrentKeycodes[6];
    ULONGLONG ModifierHoldDeadline[8];
    ULONGLONG KeyHoldDeadline[6];
    ULONG LastCommandStatus;
    ULONG LastCommandUsage;
    BOOLEAN KeyboardStateDirty;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

NTSTATUS SCMFD_Keyboard_RootCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit);

EXTERN_C_END

#endif
