/*++
    Shared protocol for SCMFD_Joystick_Root and test clients.
--*/

#ifndef DRIVERSPIKE_JOYSTICK_COMMON_H
#define DRIVERSPIKE_JOYSTICK_COMMON_H

#include <devioctl.h>
#include <guiddef.h>
#include <pshpack1.h>

#define SCMFD_JOYSTICK_ROOT_VID 0xDEED
#define SCMFD_JOYSTICK_ROOT_PID_LEGACY 0xF00D
#define SCMFD_JOYSTICK_ROOT_PID_A 0xF10A
#define SCMFD_JOYSTICK_ROOT_PID_B 0xF00B
#define SCMFD_JOYSTICK_ROOT_PID SCMFD_JOYSTICK_ROOT_PID_A
#define SCMFD_JOYSTICK_ROOT_VERSION_NUMBER 0x0101

#define JOYSTICK_CONTROL_COLLECTION_USAGE_PAGE 0xFF00
#define JOYSTICK_CONTROL_COLLECTION_USAGE      0x0001
#define JOYSTICK_CONTROL_COLLECTION_REPORT_ID  0x01
#define JOYSTICK_COLLECTION_REPORT_ID          0x02

#define SCMFD_JOYSTICK_ROOT_AXIS_COUNT 8
#define SCMFD_JOYSTICK_ROOT_BUTTON_BYTES 16
#define SCMFD_JOYSTICK_ROOT_BUTTON_COUNT 128

#define SCMFD_JOYSTICK_ROOT_CONTROL_VERSION 1u
#define SCMFD_JOYSTICK_ROOT_FEATURE_VERSION 1u
#define SCMFD_JOYSTICK_ROOT_FEATURE_GET_STATS 0x01u
#define SCMFD_JOYSTICK_ROOT_FEATURE_BUTTON    0x02u
#define SCMFD_JOYSTICK_ROOT_FEATURE_AXIS      0x03u
#define SCMFD_JOYSTICK_ROOT_FEATURE_RELEASE_ALL 0x04u

#define SCMFD_JOYSTICK_ROOT_STATUS_OK                 0u
#define SCMFD_JOYSTICK_ROOT_STATUS_INVALID_PARAMETERS 1u
#define SCMFD_JOYSTICK_ROOT_STATUS_INVALID_BUTTON     2u
#define SCMFD_JOYSTICK_ROOT_STATUS_INVALID_AXIS       3u

EXTERN_C const GUID GUID_DEVINTERFACE_SCMFD_JOYSTICK_ROOT_CONTROL;

typedef struct _SCMFD_JOYSTICK_ROOT_INPUT_REPORT {
    unsigned char ReportId;
    short Axes[SCMFD_JOYSTICK_ROOT_AXIS_COUNT];
    unsigned char Buttons[SCMFD_JOYSTICK_ROOT_BUTTON_BYTES];
} SCMFD_JOYSTICK_ROOT_INPUT_REPORT, *PSCMFD_JOYSTICK_ROOT_INPUT_REPORT;

#define SCMFD_JOYSTICK_ROOT_INPUT_REPORT_SIZE_CB ((unsigned short)sizeof(SCMFD_JOYSTICK_ROOT_INPUT_REPORT))

typedef struct _SCMFD_JOYSTICK_ROOT_STATS_OUTPUT {
    unsigned int StructSize;
    unsigned int Version;
    unsigned int Sequence;
    unsigned int ButtonCommandCount;
    unsigned int AxisCommandCount;
    unsigned int ReleaseAllCount;
    unsigned int ReportsCompletedCount;
    unsigned int EmptyTicksCount;
    unsigned int LastCompletionStatus;
    unsigned int LastCommandStatus;
    unsigned int LastButton;
    unsigned int LastAxis;
    unsigned char LastReport[SCMFD_JOYSTICK_ROOT_INPUT_REPORT_SIZE_CB];
    SCMFD_JOYSTICK_ROOT_INPUT_REPORT CurrentReport;
} SCMFD_JOYSTICK_ROOT_STATS_OUTPUT, *PSCMFD_JOYSTICK_ROOT_STATS_OUTPUT;

typedef struct _SCMFD_JOYSTICK_ROOT_BUTTON_INPUT {
    unsigned int StructSize;
    unsigned int Version;
    unsigned int Button;
    unsigned int Pressed;
} SCMFD_JOYSTICK_ROOT_BUTTON_INPUT, *PSCMFD_JOYSTICK_ROOT_BUTTON_INPUT;

typedef struct _SCMFD_JOYSTICK_ROOT_AXIS_INPUT {
    unsigned int StructSize;
    unsigned int Version;
    unsigned int Axis;
    int Value;
} SCMFD_JOYSTICK_ROOT_AXIS_INPUT, *PSCMFD_JOYSTICK_ROOT_AXIS_INPUT;

#define SCMFD_JOYSTICK_ROOT_FEATURE_PAYLOAD_SIZE \
    ((sizeof(SCMFD_JOYSTICK_ROOT_STATS_OUTPUT) > sizeof(SCMFD_JOYSTICK_ROOT_BUTTON_INPUT) && \
      sizeof(SCMFD_JOYSTICK_ROOT_STATS_OUTPUT) > sizeof(SCMFD_JOYSTICK_ROOT_AXIS_INPUT)) ? \
        sizeof(SCMFD_JOYSTICK_ROOT_STATS_OUTPUT) : \
        ((sizeof(SCMFD_JOYSTICK_ROOT_BUTTON_INPUT) > sizeof(SCMFD_JOYSTICK_ROOT_AXIS_INPUT)) ? \
            sizeof(SCMFD_JOYSTICK_ROOT_BUTTON_INPUT) : sizeof(SCMFD_JOYSTICK_ROOT_AXIS_INPUT)))

typedef struct _SCMFD_JOYSTICK_ROOT_FEATURE_REPORT {
    unsigned char ReportId;
    unsigned char ControlCode;
    unsigned short Version;
    union {
        SCMFD_JOYSTICK_ROOT_BUTTON_INPUT Button;
        SCMFD_JOYSTICK_ROOT_AXIS_INPUT Axis;
        SCMFD_JOYSTICK_ROOT_STATS_OUTPUT Stats;
        unsigned char Raw[SCMFD_JOYSTICK_ROOT_FEATURE_PAYLOAD_SIZE];
    } Payload;
} SCMFD_JOYSTICK_ROOT_FEATURE_REPORT, *PSCMFD_JOYSTICK_ROOT_FEATURE_REPORT;

#define SCMFD_JOYSTICK_ROOT_FEATURE_REPORT_SIZE_CB ((unsigned short)sizeof(SCMFD_JOYSTICK_ROOT_FEATURE_REPORT))

#include <poppack.h>

#endif
