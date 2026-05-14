/*++ Manual queue + timer for IOCTL_HID_READ_REPORT (vhidmini2 pattern). --*/

#ifndef MANUALQUEUE_H
#define MANUALQUEUE_H

#include <wdf.h>
#include "device.h"

EXTERN_C_START

typedef struct _MANUAL_QUEUE_CONTEXT {
    WDFQUEUE Queue;
    PDEVICE_CONTEXT DeviceContext;
    WDFTIMER Timer;
    WDFREQUEST LastCompletedReadRequest;
} MANUAL_QUEUE_CONTEXT, *PMANUAL_QUEUE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(MANUAL_QUEUE_CONTEXT, GetManualQueueContext)

NTSTATUS
SCMFD_Joystick_A_RootManualQueueInitialize(
    _In_ WDFDEVICE Device,
    _Out_ WDFQUEUE *Queue
    );

EVT_WDF_TIMER SCMFD_Joystick_A_RootEvtTimerFunc;
VOID SCMFD_Joystick_A_RootKickManualQueue(_In_ PDEVICE_CONTEXT DeviceContext, _In_ ULONG DelayMs);

EXTERN_C_END

#endif
