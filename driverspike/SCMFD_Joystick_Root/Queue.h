/*++ Default I/O queue (vhidmini2: UMDF receives HID IOCTLs via EvtIoDeviceControl). --*/

#ifndef QUEUE_H
#define QUEUE_H

#include <wdf.h>
#include "device.h"

EXTERN_C_START

typedef struct _QUEUE_CONTEXT {
    WDFQUEUE Queue;
    PDEVICE_CONTEXT DeviceContext;
    UCHAR OutputReport;
} QUEUE_CONTEXT, *PQUEUE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(QUEUE_CONTEXT, QueueGetContext)

NTSTATUS SCMFD_Joystick_RootQueueInitialize(_In_ WDFDEVICE Device);

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL SCMFD_Joystick_RootEvtIoDeviceControl;
EVT_WDF_IO_QUEUE_IO_STOP SCMFD_Joystick_RootEvtIoStop;

EXTERN_C_END

#endif
