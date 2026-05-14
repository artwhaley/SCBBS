/*++ Device context for SCMFD_Joystick_Root. --*/

#ifndef DEVICE_H
#define DEVICE_H

#include <wdf.h>
#include <hidport.h>
#include "public.h"
#include "../driverspike-joystick-common.h"

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
    ULONG DebugInstanceOrdinal;
    WDFREQUEST LastForwardedReadRequest;
    ULONG LastForwardedReadIoctl;
    ULONG Sequence;
    ULONG ButtonCommandCount;
    ULONG AxisCommandCount;
    ULONG ReleaseAllCount;
    ULONG TimerTickCount;
    ULONG ReportsCompletedCount;
    ULONG EmptyTickCount;
    NTSTATUS LastCompletionStatus;
    ULONG LastCommandStatus;
    ULONG LastButton;
    ULONG LastAxis;
    SCMFD_JOYSTICK_ROOT_INPUT_REPORT CurrentReport;
    SCMFD_JOYSTICK_ROOT_INPUT_REPORT LastCompletedReport;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

NTSTATUS SCMFD_Joystick_RootCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit);

EXTERN_C_END

#endif
