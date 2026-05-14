/*++

Module Name:

    driver.h

Abstract:

    This file contains the driver definitions.

Environment:

    User-mode Driver Framework 2

--*/

#include <windows.h>
#include <wdf.h>

#include "device.h"
#include "queue.h"
#include "manualqueue.h"
#include "hid.h"
#include "trace.h"

EXTERN_C_START

//
// WDFDRIVER Events
//

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD SCMFD_Joystick_RootEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP SCMFD_Joystick_RootEvtDriverContextCleanup;

EXTERN_C_END
