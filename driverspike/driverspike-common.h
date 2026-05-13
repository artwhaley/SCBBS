/*++
    Shared protocol between SCMFD_Keyboard_Root and TestEnumeratorAndClient.
    Values aligned with Microsoft vhidmini2 inc/common.h (phase 1 parity).
--*/

#ifndef DRIVERSPIKE_COMMON_H
#define DRIVERSPIKE_COMMON_H

#include <devioctl.h>
#include <guiddef.h>
#include <pshpack1.h>

#define HIDMINI_CONTROL_CODE_SET_ATTRIBUTES 0x00
#define HIDMINI_CONTROL_CODE_DUMMY1         0x01
#define HIDMINI_CONTROL_CODE_DUMMY2         0x02

#define CONTROL_COLLECTION_USAGE_PAGE 0xFF00
#define CONTROL_COLLECTION_USAGE      0x0001
#define CONTROL_COLLECTION_REPORT_ID  0x01
#define KEYBOARD_COLLECTION_REPORT_ID 0x02
#define TEST_COLLECTION_REPORT_ID     0x03

#define MAXIMUM_STRING_LENGTH (126 * sizeof(unsigned short))

typedef struct _MY_DEVICE_ATTRIBUTES {
    unsigned short VendorID;
    unsigned short ProductID;
    unsigned short VersionNumber;
} MY_DEVICE_ATTRIBUTES, *PMY_DEVICE_ATTRIBUTES;

typedef struct _HIDMINI_CONTROL_INFO {
    unsigned char ReportId;
    unsigned char ControlCode;
    union {
        MY_DEVICE_ATTRIBUTES Attributes;
        struct {
            unsigned long Dummy1;
            unsigned long Dummy2;
        } Dummy;
    } u;
} HIDMINI_CONTROL_INFO, *PHIDMINI_CONTROL_INFO;

typedef struct _HIDMINI_INPUT_REPORT {
    unsigned char Modifier;
    unsigned char Reserved;
    unsigned char Keycodes[6];
} HIDMINI_INPUT_REPORT, *PHIDMINI_INPUT_REPORT;

typedef struct _HIDMINI_KEYBOARD_INPUT_REPORT {
    unsigned char ReportId;
    HIDMINI_INPUT_REPORT Keyboard;
} HIDMINI_KEYBOARD_INPUT_REPORT, *PHIDMINI_KEYBOARD_INPUT_REPORT;

typedef struct _HIDMINI_OUTPUT_REPORT {
    unsigned char ReportId;
    unsigned char Data;
    unsigned short Pad1;
    unsigned long Pad2;
} HIDMINI_OUTPUT_REPORT, *PHIDMINI_OUTPUT_REPORT;

#include <poppack.h>

#define FEATURE_REPORT_SIZE_CB ((unsigned short)(sizeof(HIDMINI_CONTROL_INFO) - 1))
#define INPUT_REPORT_SIZE_CB   ((unsigned short)(sizeof(HIDMINI_KEYBOARD_INPUT_REPORT)))
#define OUTPUT_REPORT_SIZE_CB  ((unsigned short)(sizeof(HIDMINI_OUTPUT_REPORT) - 1))

#define SCMFD_KEYBOARD_ROOT_CONTROL_VERSION 1u
#define SCMFD_KEYBOARD_ROOT_FEATURE_VERSION 1u
#define SCMFD_KEYBOARD_ROOT_FEATURE_GET_STATS    0x01u
#define SCMFD_KEYBOARD_ROOT_FEATURE_KEY_DOWN     0x02u
#define SCMFD_KEYBOARD_ROOT_FEATURE_KEY_UP       0x03u
#define SCMFD_KEYBOARD_ROOT_FEATURE_RELEASE_ALL  0x04u

#define SCMFD_KEYBOARD_ROOT_COMMAND_STATUS_OK                 0u
#define SCMFD_KEYBOARD_ROOT_COMMAND_STATUS_INVALID_USAGE      1u
#define SCMFD_KEYBOARD_ROOT_COMMAND_STATUS_ROLLOVER_FULL      2u
#define SCMFD_KEYBOARD_ROOT_COMMAND_STATUS_AUTO_RELEASED      3u
#define SCMFD_KEYBOARD_ROOT_COMMAND_STATUS_INVALID_PARAMETERS 4u

EXTERN_C const GUID GUID_DEVINTERFACE_SCMFD_KEYBOARD_ROOT_CONTROL;

#define IOCTL_SCMFD_KEYBOARD_ROOT_INJECT_REPORT \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_SCMFD_KEYBOARD_ROOT_GET_STATS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_SCMFD_KEYBOARD_ROOT_SET_MODE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_WRITE_ACCESS)

typedef struct _SCMFD_KEYBOARD_ROOT_KEY_EVENT_INPUT {
    unsigned int StructSize;
    unsigned int Version;
    unsigned short Usage;
    unsigned short Flags;
    unsigned int MaxHoldMs;
} SCMFD_KEYBOARD_ROOT_KEY_EVENT_INPUT, *PSCMFD_KEYBOARD_ROOT_KEY_EVENT_INPUT;

typedef struct _SCMFD_KEYBOARD_ROOT_SET_MODE_INPUT {
    unsigned int StructSize;
    unsigned int Version;
    unsigned int Mode;
} SCMFD_KEYBOARD_ROOT_SET_MODE_INPUT, *PSCMFD_KEYBOARD_ROOT_SET_MODE_INPUT;

typedef struct _SCMFD_KEYBOARD_ROOT_STATS_OUTPUT {
    unsigned int StructSize;
    unsigned int Version;
    unsigned int Sequence;
    unsigned int KeyDownCount;
    unsigned int KeyUpCount;
    unsigned int ReleaseAllCount;
    unsigned int AutoReleaseCount;
    unsigned int DuplicateNoOpCount;
    unsigned int RolloverRejectCount;
    unsigned int ReportsCompletedCount;
    unsigned int EmptyTicksCount;
    unsigned int LastCompletionStatus;
    unsigned int LastCommandStatus;
    unsigned int LastCommandUsage;
    unsigned char LastReport[INPUT_REPORT_SIZE_CB];
    unsigned char CurrentModifier;
    unsigned char CurrentKeycodes[6];
} SCMFD_KEYBOARD_ROOT_STATS_OUTPUT, *PSCMFD_KEYBOARD_ROOT_STATS_OUTPUT;

#define SCMFD_KEYBOARD_ROOT_FEATURE_PAYLOAD_SIZE \
    ((sizeof(SCMFD_KEYBOARD_ROOT_STATS_OUTPUT) > sizeof(SCMFD_KEYBOARD_ROOT_KEY_EVENT_INPUT)) ? \
        sizeof(SCMFD_KEYBOARD_ROOT_STATS_OUTPUT) : sizeof(SCMFD_KEYBOARD_ROOT_KEY_EVENT_INPUT))

typedef struct _SCMFD_KEYBOARD_ROOT_FEATURE_REPORT {
    unsigned char ReportId;
    unsigned char ControlCode;
    unsigned short Version;
    union {
        SCMFD_KEYBOARD_ROOT_KEY_EVENT_INPUT KeyEvent;
        SCMFD_KEYBOARD_ROOT_STATS_OUTPUT Stats;
        unsigned char Raw[SCMFD_KEYBOARD_ROOT_FEATURE_PAYLOAD_SIZE];
    } Payload;
} SCMFD_KEYBOARD_ROOT_FEATURE_REPORT, *PSCMFD_KEYBOARD_ROOT_FEATURE_REPORT;

#define SCMFD_KEYBOARD_ROOT_FEATURE_REPORT_SIZE_CB ((unsigned short)sizeof(SCMFD_KEYBOARD_ROOT_FEATURE_REPORT))

#endif
