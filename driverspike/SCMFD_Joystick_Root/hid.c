/*++ Minimal joystick HID descriptor + IOCTL dispatch for SCMFD_Joystick_Root. --*/

#include "driver.h"
#include <hidusage.h>
#include "hid.tmh"

static const WCHAR g_Manufacturer[] = L"SCMFD";
#define SCMFD_JOYSTICK_DEVICE_STRING_INDEX 5

HID_REPORT_DESCRIPTOR G_DefaultReportDescriptor[] = {
    0x05, 0x01,                    // Usage Page (Generic Desktop)
    0x09, 0x04,                    // Usage (Joystick)
    0xA1, 0x01,                    // Collection (Application)
    0x85, JOYSTICK_COLLECTION_REPORT_ID, //   Report ID (2)

    0x05, 0x01,                    //   Usage Page (Generic Desktop)
    0x16, 0x00, 0x80,              //   Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,              //   Logical Maximum (32767)
    0x09, 0x30,                    //   Usage (X)
    0x09, 0x31,                    //   Usage (Y)
    0x09, 0x32,                    //   Usage (Z)
    0x09, 0x33,                    //   Usage (Rx)
    0x09, 0x34,                    //   Usage (Ry)
    0x09, 0x35,                    //   Usage (Rz)
    0x09, 0x36,                    //   Usage (Slider)
    0x09, 0x37,                    //   Usage (Dial)
    0x75, 0x10,                    //   Report Size (16)
    0x95, 0x08,                    //   Report Count (8)
    0x81, 0x02,                    //   Input (Data, Variable, Absolute)

    0x15, 0x00,                    //   Logical Minimum (0) for buttons
    0x25, 0x01,                    //   Logical Maximum (1)
    0x05, 0x09,                    //   Usage Page (Button)
    0x19, 0x01,                    //   Usage Minimum (Button 1)
    0x2A, 0x80, 0x00,              //   Usage Maximum (Button 128)
    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x80,                    //   Report Count (128)
    0x81, 0x02,                    //   Input (Data, Variable, Absolute)
    0xC0,                          // End Collection

    0x06, 0x00, 0xFF,              // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,                    // Usage (0x01)
    0xA1, 0x01,                    // Collection (Application)
    0x85, JOYSTICK_CONTROL_COLLECTION_REPORT_ID, // Report ID (1)
    0x09, 0x02,                    //   Usage (0x02)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x75, 0x08,                    //   Report Size (8)
    0x95, SCMFD_JOYSTICK_ROOT_FEATURE_REPORT_SIZE_CB - 1,
    0xB1, 0x02,                    //   Feature (Data, Variable, Absolute)
    0xC0                           // End Collection
};

HID_DESCRIPTOR G_DefaultHidDescriptor = {
    0x09,
    0x21,
    0x0100,
    0x00,
    0x01,
    {
        0x22,
        sizeof(G_DefaultReportDescriptor)
    }
};

static PCSTR GetIoctlName(_In_ ULONG IoControlCode)
{
    switch (IoControlCode) {
    case IOCTL_HID_GET_DEVICE_DESCRIPTOR: return "IOCTL_HID_GET_DEVICE_DESCRIPTOR";
    case IOCTL_HID_GET_DEVICE_ATTRIBUTES: return "IOCTL_HID_GET_DEVICE_ATTRIBUTES";
    case IOCTL_HID_GET_REPORT_DESCRIPTOR: return "IOCTL_HID_GET_REPORT_DESCRIPTOR";
    case IOCTL_HID_READ_REPORT: return "IOCTL_HID_READ_REPORT";
    case IOCTL_HID_WRITE_REPORT: return "IOCTL_HID_WRITE_REPORT";
    case IOCTL_UMDF_HID_GET_FEATURE: return "IOCTL_UMDF_HID_GET_FEATURE";
    case IOCTL_UMDF_HID_SET_FEATURE: return "IOCTL_UMDF_HID_SET_FEATURE";
    case IOCTL_UMDF_HID_GET_INPUT_REPORT: return "IOCTL_UMDF_HID_GET_INPUT_REPORT";
    case IOCTL_UMDF_HID_SET_OUTPUT_REPORT: return "IOCTL_UMDF_HID_SET_OUTPUT_REPORT";
    case IOCTL_HID_GET_STRING: return "IOCTL_HID_GET_STRING";
    case IOCTL_HID_GET_INDEXED_STRING: return "IOCTL_HID_GET_INDEXED_STRING";
    case IOCTL_HID_ACTIVATE_DEVICE: return "IOCTL_HID_ACTIVATE_DEVICE";
    case IOCTL_HID_DEACTIVATE_DEVICE: return "IOCTL_HID_DEACTIVATE_DEVICE";
    case IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST: return "IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST";
    case IOCTL_GET_PHYSICAL_DESCRIPTOR: return "IOCTL_GET_PHYSICAL_DESCRIPTOR";
    default: return "UNKNOWN_IOCTL";
    }
}

NTSTATUS RequestCopyFromBuffer(_In_ WDFREQUEST Request, _In_ PVOID SourceBuffer, _In_ size_t NumBytesToCopyFrom)
{
    NTSTATUS status;
    WDFMEMORY memory;
    size_t outputBufferLength;

    status = WdfRequestRetrieveOutputMemory(Request, &memory);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    WdfMemoryGetBuffer(memory, &outputBufferLength);
    if (outputBufferLength < NumBytesToCopyFrom) {
        return STATUS_INVALID_BUFFER_SIZE;
    }
    status = WdfMemoryCopyFromBuffer(memory, 0, SourceBuffer, NumBytesToCopyFrom);
    if (NT_SUCCESS(status)) {
        WdfRequestSetInformation(Request, NumBytesToCopyFrom);
    }
    return status;
}

NTSTATUS CompleteHidReadRequest(_In_ WDFREQUEST Request, _In_reads_bytes_(NumBytes) PVOID SourceBuffer, _In_ size_t NumBytes)
{
    NTSTATUS status;
    WDFMEMORY outputMemory;
    size_t outputBufferLength;
    PVOID destinationBuffer;

    status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    destinationBuffer = WdfMemoryGetBuffer(outputMemory, &outputBufferLength);
    if (outputBufferLength < NumBytes) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    RtlZeroMemory(destinationBuffer, outputBufferLength);
    status = WdfMemoryCopyFromBuffer(outputMemory, 0, SourceBuffer, NumBytes);
    if (NT_SUCCESS(status)) {
        WdfRequestSetInformation(Request, NumBytes);
    }
    return status;
}

static NTSTATUS GetStringId(_In_ WDFREQUEST Request, _Out_ ULONG *StringId, _Out_ ULONG *LanguageId)
{
    NTSTATUS status;
    WDFMEMORY inputMemory;
    size_t inputBufferLength;
    PVOID inputBuffer;
    ULONG inputValue;

    status = WdfRequestRetrieveInputMemory(Request, &inputMemory);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    inputBuffer = WdfMemoryGetBuffer(inputMemory, &inputBufferLength);
    if (inputBufferLength < sizeof(ULONG)) {
        return STATUS_INVALID_BUFFER_SIZE;
    }
    inputValue = (*(PULONG)inputBuffer);
    *StringId = (inputValue & 0x0ffff);
    *LanguageId = (inputValue >> 16);
    return STATUS_SUCCESS;
}

NTSTATUS GetIndexedString(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request)
{
    NTSTATUS status;
    ULONG languageId;
    ULONG stringIndex;

    status = GetStringId(Request, &stringIndex, &languageId);
    UNREFERENCED_PARAMETER(languageId);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    if (stringIndex != SCMFD_JOYSTICK_DEVICE_STRING_INDEX) {
        return STATUS_INVALID_PARAMETER;
    }
    return RequestCopyFromBuffer(
        Request,
        (PVOID)QueueContext->DeviceContext->IndexedString,
        (wcslen(QueueContext->DeviceContext->IndexedString) + 1) * sizeof(WCHAR));
}

NTSTATUS GetString(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request)
{
    NTSTATUS status;
    ULONG languageId;
    ULONG stringId;
    size_t stringSizeCb;
    PCWSTR string;

    status = GetStringId(Request, &stringId, &languageId);
    UNREFERENCED_PARAMETER(languageId);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    switch (stringId) {
    case HID_STRING_ID_IMANUFACTURER:
        stringSizeCb = sizeof(g_Manufacturer);
        string = g_Manufacturer;
        break;
    case HID_STRING_ID_IPRODUCT:
        string = QueueContext->DeviceContext->ProductString;
        stringSizeCb = (wcslen(string) + 1) * sizeof(WCHAR);
        break;
    case HID_STRING_ID_ISERIALNUMBER:
        string = QueueContext->DeviceContext->SerialString;
        stringSizeCb = (wcslen(string) + 1) * sizeof(WCHAR);
        break;
    default:
        return STATUS_INVALID_PARAMETER;
    }
    return RequestCopyFromBuffer(Request, (PVOID)string, stringSizeCb);
}

static VOID FillStats(_In_ PDEVICE_CONTEXT DeviceContext, _Out_ PSCMFD_JOYSTICK_ROOT_STATS_OUTPUT Stats)
{
    RtlZeroMemory(Stats, sizeof(*Stats));
    Stats->StructSize = sizeof(*Stats);
    Stats->Version = SCMFD_JOYSTICK_ROOT_CONTROL_VERSION;
    Stats->Sequence = DeviceContext->Sequence;
    Stats->ButtonCommandCount = DeviceContext->ButtonCommandCount;
    Stats->AxisCommandCount = DeviceContext->AxisCommandCount;
    Stats->ReleaseAllCount = DeviceContext->ReleaseAllCount;
    Stats->ReportsCompletedCount = DeviceContext->ReportsCompletedCount;
    Stats->EmptyTicksCount = DeviceContext->EmptyTickCount;
    Stats->LastCompletionStatus = (ULONG)DeviceContext->LastCompletionStatus;
    Stats->LastCommandStatus = DeviceContext->LastCommandStatus;
    Stats->LastButton = DeviceContext->LastButton;
    Stats->LastAxis = DeviceContext->LastAxis;
    RtlCopyMemory(Stats->LastReport, &DeviceContext->LastCompletedReport, sizeof(Stats->LastReport));
    RtlCopyMemory(&Stats->CurrentReport, &DeviceContext->CurrentReport, sizeof(Stats->CurrentReport));
}

static VOID MarkCommandResult(_In_ PDEVICE_CONTEXT DeviceContext, _In_ ULONG Status)
{
    DeviceContext->LastCommandStatus = Status;
    DeviceContext->Sequence++;
}

static NTSTATUS HandleButtonCommand(_In_ PDEVICE_CONTEXT DeviceContext, _In_ PSCMFD_JOYSTICK_ROOT_BUTTON_INPUT Input)
{
    ULONG zeroBased;
    UCHAR mask;

    if (Input->StructSize < sizeof(*Input) || Input->Version != SCMFD_JOYSTICK_ROOT_CONTROL_VERSION) {
        MarkCommandResult(DeviceContext, SCMFD_JOYSTICK_ROOT_STATUS_INVALID_PARAMETERS);
        return STATUS_INVALID_PARAMETER;
    }
    if (Input->Button < 1 || Input->Button > SCMFD_JOYSTICK_ROOT_BUTTON_COUNT) {
        MarkCommandResult(DeviceContext, SCMFD_JOYSTICK_ROOT_STATUS_INVALID_BUTTON);
        return STATUS_INVALID_PARAMETER;
    }

    zeroBased = Input->Button - 1;
    mask = (UCHAR)(1u << (zeroBased % 8));
    if (Input->Pressed != 0) {
        DeviceContext->CurrentReport.Buttons[zeroBased / 8] |= mask;
    } else {
        DeviceContext->CurrentReport.Buttons[zeroBased / 8] &= (UCHAR)~mask;
    }

    DeviceContext->CurrentReport.ReportId = JOYSTICK_COLLECTION_REPORT_ID;
    DeviceContext->ButtonCommandCount++;
    DeviceContext->LastButton = Input->Button;
    MarkCommandResult(DeviceContext, SCMFD_JOYSTICK_ROOT_STATUS_OK);
    return STATUS_SUCCESS;
}

static NTSTATUS HandleAxisCommand(_In_ PDEVICE_CONTEXT DeviceContext, _In_ PSCMFD_JOYSTICK_ROOT_AXIS_INPUT Input)
{
    if (Input->StructSize < sizeof(*Input) || Input->Version != SCMFD_JOYSTICK_ROOT_CONTROL_VERSION) {
        MarkCommandResult(DeviceContext, SCMFD_JOYSTICK_ROOT_STATUS_INVALID_PARAMETERS);
        return STATUS_INVALID_PARAMETER;
    }
    if (Input->Axis >= SCMFD_JOYSTICK_ROOT_AXIS_COUNT || Input->Value < -32768 || Input->Value > 32767) {
        MarkCommandResult(DeviceContext, SCMFD_JOYSTICK_ROOT_STATUS_INVALID_AXIS);
        return STATUS_INVALID_PARAMETER;
    }

    DeviceContext->CurrentReport.ReportId = JOYSTICK_COLLECTION_REPORT_ID;
    DeviceContext->CurrentReport.Axes[Input->Axis] = (SHORT)Input->Value;
    DeviceContext->AxisCommandCount++;
    DeviceContext->LastAxis = Input->Axis;
    MarkCommandResult(DeviceContext, SCMFD_JOYSTICK_ROOT_STATUS_OK);
    return STATUS_SUCCESS;
}

static NTSTATUS HandleReleaseAllCommand(_In_ PDEVICE_CONTEXT DeviceContext)
{
    RtlZeroMemory(&DeviceContext->CurrentReport, sizeof(DeviceContext->CurrentReport));
    DeviceContext->CurrentReport.ReportId = JOYSTICK_COLLECTION_REPORT_ID;
    DeviceContext->ReleaseAllCount++;
    MarkCommandResult(DeviceContext, SCMFD_JOYSTICK_ROOT_STATUS_OK);
    return STATUS_SUCCESS;
}

NTSTATUS ReadReport(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request, _Always_(_Out_) BOOLEAN *CompleteRequest)
{
    NTSTATUS status;

    status = WdfRequestForwardToIoQueue(Request, QueueContext->DeviceContext->ManualQueue);
    if (!NT_SUCCESS(status)) {
        *CompleteRequest = TRUE;
        return status;
    }

    QueueContext->DeviceContext->LastForwardedReadRequest = Request;
    QueueContext->DeviceContext->LastForwardedReadIoctl = IOCTL_HID_READ_REPORT;
    *CompleteRequest = FALSE;
    return STATUS_SUCCESS;
}

NTSTATUS GetInputReport(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request, _Always_(_Out_) BOOLEAN *CompleteRequest)
{
    NTSTATUS status;
    HID_XFER_PACKET packet;
    SCMFD_JOYSTICK_ROOT_INPUT_REPORT report;

    status = RequestGetHidXferPacket_ToReadFromDevice(Request, &packet);
    if (!NT_SUCCESS(status)) {
        *CompleteRequest = TRUE;
        return status;
    }
    if (packet.reportId != 0 && packet.reportId != JOYSTICK_COLLECTION_REPORT_ID) {
        *CompleteRequest = TRUE;
        return STATUS_INVALID_PARAMETER;
    }
    if (packet.reportBufferLen < sizeof(SCMFD_JOYSTICK_ROOT_INPUT_REPORT)) {
        *CompleteRequest = TRUE;
        return STATUS_INVALID_BUFFER_SIZE;
    }

    report = QueueContext->DeviceContext->CurrentReport;
    report.ReportId = JOYSTICK_COLLECTION_REPORT_ID;
    status = CompleteHidReadRequest(Request, &report, sizeof(report));
    if (NT_SUCCESS(status)) {
        QueueContext->DeviceContext->Sequence++;
        QueueContext->DeviceContext->ReportsCompletedCount++;
        QueueContext->DeviceContext->LastCompletedReport = report;
        QueueContext->DeviceContext->LastCompletionStatus = STATUS_SUCCESS;
    } else {
        QueueContext->DeviceContext->LastCompletionStatus = status;
    }
    *CompleteRequest = TRUE;
    return status;
}

NTSTATUS WriteReport(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request)
{
    UNREFERENCED_PARAMETER(QueueContext);
    UNREFERENCED_PARAMETER(Request);
    WdfRequestSetInformation(Request, 0);
    return STATUS_SUCCESS;
}

NTSTATUS SetOutputReport(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request)
{
    UNREFERENCED_PARAMETER(QueueContext);
    UNREFERENCED_PARAMETER(Request);
    WdfRequestSetInformation(Request, 0);
    return STATUS_SUCCESS;
}

NTSTATUS GetFeature(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request)
{
    NTSTATUS status;
    PSCMFD_JOYSTICK_ROOT_FEATURE_REPORT outputPacket;
    size_t outputLength;

    status = WdfRequestRetrieveOutputBuffer(Request, SCMFD_JOYSTICK_ROOT_FEATURE_REPORT_SIZE_CB, (PVOID*)&outputPacket, &outputLength);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    RtlZeroMemory(outputPacket, outputLength);
    outputPacket->ReportId = JOYSTICK_CONTROL_COLLECTION_REPORT_ID;
    outputPacket->ControlCode = SCMFD_JOYSTICK_ROOT_FEATURE_GET_STATS;
    outputPacket->Version = SCMFD_JOYSTICK_ROOT_FEATURE_VERSION;
    FillStats(QueueContext->DeviceContext, &outputPacket->Payload.Stats);
    WdfRequestSetInformation(Request, SCMFD_JOYSTICK_ROOT_FEATURE_REPORT_SIZE_CB);
    return STATUS_SUCCESS;
}

NTSTATUS SetFeature(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request)
{
    NTSTATUS status;
    PSCMFD_JOYSTICK_ROOT_FEATURE_REPORT inputPacket;
    size_t inputLength;

    status = WdfRequestRetrieveInputBuffer(Request, sizeof(SCMFD_JOYSTICK_ROOT_FEATURE_REPORT), (PVOID*)&inputPacket, &inputLength);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    if (inputLength < sizeof(SCMFD_JOYSTICK_ROOT_FEATURE_REPORT) ||
        inputPacket->ReportId != JOYSTICK_CONTROL_COLLECTION_REPORT_ID ||
        inputPacket->Version != SCMFD_JOYSTICK_ROOT_FEATURE_VERSION) {
        MarkCommandResult(QueueContext->DeviceContext, SCMFD_JOYSTICK_ROOT_STATUS_INVALID_PARAMETERS);
        return STATUS_INVALID_PARAMETER;
    }

    switch (inputPacket->ControlCode) {
    case SCMFD_JOYSTICK_ROOT_FEATURE_BUTTON:
        status = HandleButtonCommand(QueueContext->DeviceContext, &inputPacket->Payload.Button);
        break;
    case SCMFD_JOYSTICK_ROOT_FEATURE_AXIS:
        status = HandleAxisCommand(QueueContext->DeviceContext, &inputPacket->Payload.Axis);
        break;
    case SCMFD_JOYSTICK_ROOT_FEATURE_RELEASE_ALL:
        status = HandleReleaseAllCommand(QueueContext->DeviceContext);
        break;
    default:
        MarkCommandResult(QueueContext->DeviceContext, SCMFD_JOYSTICK_ROOT_STATUS_INVALID_PARAMETERS);
        status = STATUS_INVALID_PARAMETER;
        break;
    }

    if (NT_SUCCESS(status)) {
        SCMFD_Joystick_RootKickManualQueue(QueueContext->DeviceContext, 1);
    }
    WdfRequestSetInformation(Request, 0);
    return status;
}

VOID SCMFD_Joystick_RootEvtIoHidDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode)
{
    NTSTATUS status = STATUS_SUCCESS;
    BOOLEAN completeRequest = TRUE;
    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    PDEVICE_CONTEXT deviceContext = DeviceGetContext(device);
    PQUEUE_CONTEXT queueContext = QueueGetContext(Queue);

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
        "Joystick HID IOCTL=0x%08X (%s) req=%p", IoControlCode, GetIoctlName(IoControlCode), Request);

    switch (IoControlCode) {
    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
        status = RequestCopyFromBuffer(Request, &deviceContext->HidDescriptor, deviceContext->HidDescriptor.bLength);
        break;
    case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
        status = RequestCopyFromBuffer(Request, &deviceContext->HidDeviceAttributes, sizeof(HID_DEVICE_ATTRIBUTES));
        break;
    case IOCTL_HID_GET_REPORT_DESCRIPTOR:
        status = RequestCopyFromBuffer(Request, deviceContext->ReportDescriptor, deviceContext->HidDescriptor.DescriptorList[0].wReportLength);
        break;
    case IOCTL_HID_READ_REPORT:
        status = ReadReport(queueContext, Request, &completeRequest);
        break;
    case IOCTL_UMDF_HID_GET_INPUT_REPORT:
        status = GetInputReport(queueContext, Request, &completeRequest);
        break;
    case IOCTL_HID_WRITE_REPORT:
        status = WriteReport(queueContext, Request);
        break;
    case IOCTL_UMDF_HID_SET_OUTPUT_REPORT:
        status = SetOutputReport(queueContext, Request);
        break;
    case IOCTL_UMDF_HID_GET_FEATURE:
        status = GetFeature(queueContext, Request);
        break;
    case IOCTL_UMDF_HID_SET_FEATURE:
        status = SetFeature(queueContext, Request);
        break;
    case IOCTL_HID_GET_STRING:
        status = GetString(queueContext, Request);
        break;
    case IOCTL_HID_GET_INDEXED_STRING:
        status = GetIndexedString(queueContext, Request);
        break;
    case IOCTL_HID_ACTIVATE_DEVICE:
    case IOCTL_HID_DEACTIVATE_DEVICE:
        status = STATUS_SUCCESS;
        break;
    case IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST:
    case IOCTL_GET_PHYSICAL_DESCRIPTOR:
        status = STATUS_NOT_IMPLEMENTED;
        break;
    default:
        status = STATUS_NOT_IMPLEMENTED;
        break;
    }

    if (completeRequest) {
        WdfRequestComplete(Request, status);
    }
}
