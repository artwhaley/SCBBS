/*++ Report descriptor + HID IOCTL dispatch (vhidmini2 UMDF2). --*/

#include "driver.h"
#include <hidusage.h>
#include "hid.tmh"

static const WCHAR g_Manufacturer[] = L"UMDF Virtual hidmini device Manufacturer string";
static const WCHAR g_Product[] = L"UMDF Virtual hidmini device Product string";
static const WCHAR g_Serial[] = L"UMDF Virtual hidmini device Serial Number string";
static const WCHAR g_DeviceIndexed[] = L"UMDF Virtual hidmini device";
#define VHIDMINI_DEVICE_STRING_INDEX 5

HID_REPORT_DESCRIPTOR G_DefaultReportDescriptor[] = {
    0x05, 0x01,                    // Usage Page (Generic Desktop)
    0x09, 0x06,                    // Usage (Keyboard)
    0xA1, 0x01,                    // Collection (Application)
    0x85, KEYBOARD_COLLECTION_REPORT_ID, //   Report ID (2)
    0x05, 0x07,                    //   Usage Page (Keyboard/Keypad)
    0x19, 0xE0,                    //   Usage Minimum (Left Control)
    0x29, 0xE7,                    //   Usage Maximum (Right GUI)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0x01,                    //   Logical Maximum (1)
    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x08,                    //   Report Count (8)
    0x81, 0x02,                    //   Input (Data, Variable, Absolute)
    0x95, 0x01,                    //   Report Count (1)
    0x75, 0x08,                    //   Report Size (8)
    0x81, 0x01,                    //   Input (Constant)
    0x95, 0x06,                    //   Report Count (6)
    0x75, 0x08,                    //   Report Size (8)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x05, 0x07,                    //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,                    //   Usage Minimum (0)
    0x2A, 0xFF, 0x00,              //   Usage Maximum (255)
    0x81, 0x00,                    //   Input (Data, Array, Absolute)
    0x05, 0x08,                    //   Usage Page (LEDs)
    0x19, 0x01,                    //   Usage Minimum (Num Lock)
    0x29, 0x05,                    //   Usage Maximum (Kana)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0x01,                    //   Logical Maximum (1)
    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x05,                    //   Report Count (5)
    0x91, 0x02,                    //   Output (Data, Variable, Absolute)
    0x95, 0x01,                    //   Report Count (1)
    0x75, 0x03,                    //   Report Size (3)
    0x91, 0x01,                    //   Output (Constant)
    0xC0,                          // End Collection

    0x06, 0x00, 0xFF,              // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,                    // Usage (0x01)
    0xA1, 0x01,                    // Collection (Application)
    0x85, CONTROL_COLLECTION_REPORT_ID, //   Report ID (1)
    0x09, 0x02,                    //   Usage (0x02)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x75, 0x08,                    //   Report Size (8)
    0x95, THESPIKEYDRIVER_FEATURE_REPORT_SIZE_CB - 1, // Report Count (payload bytes after Report ID)
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

static volatile LONG g_HidIoctlTotalCount;
static volatile LONG g_HidReadReportIoctlCount;
static volatile LONG g_HidGetInputReportIoctlCount;
static volatile LONG g_HidWriteReportIoctlCount;
static volatile LONG g_HidSetOutputReportIoctlCount;
static volatile LONG g_HidGetFeatureIoctlCount;
static volatile LONG g_HidSetFeatureIoctlCount;
static volatile LONG g_HidGetAttrsIoctlCount;
static volatile LONG g_HidGetDescriptorIoctlCount;
static volatile LONG g_HidNotImplementedIoctlCount;
static volatile LONG g_HidUnhandledIoctlCount;

static
PCSTR
GetIoctlName(
    _In_ ULONG IoControlCode
    )
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

static
VOID
GetReportDescriptorStats(
    _In_reads_bytes_(DescriptorLength) PHID_REPORT_DESCRIPTOR Descriptor,
    _In_ USHORT DescriptorLength,
    _Out_ PULONG AppCollectionCount,
    _Out_ PULONG ReportIdItemCount
    )
{
    USHORT i;
    ULONG appCollections = 0;
    ULONG reportIds = 0;

    for (i = 0; i + 1 < DescriptorLength; i++) {
        if (Descriptor[i] == 0xA1 && Descriptor[i + 1] == 0x01) {
            appCollections++;
        }
        if (Descriptor[i] == 0x85) {
            reportIds++;
        }
    }

    *AppCollectionCount = appCollections;
    *ReportIdItemCount = reportIds;
}

NTSTATUS
RequestCopyFromBuffer(
    _In_ WDFREQUEST Request,
    _In_ PVOID SourceBuffer,
    _In_ size_t NumBytesToCopyFrom
    )
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
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_HID, "CopyOut too small cap=%Iu need=%Iu", outputBufferLength, NumBytesToCopyFrom);
        return STATUS_INVALID_BUFFER_SIZE;
    }
    status = WdfMemoryCopyFromBuffer(memory, 0, SourceBuffer, NumBytesToCopyFrom);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    WdfRequestSetInformation(Request, NumBytesToCopyFrom);
    return status;
}

NTSTATUS
CompleteHidReadRequest(
    _In_ WDFREQUEST Request,
    _In_reads_bytes_(NumBytes) PVOID SourceBuffer,
    _In_ size_t NumBytes
    )
{
    NTSTATUS status;
    WDFMEMORY outputMemory;
    size_t outputBufferLength;
    PVOID destinationBuffer = NULL;
    size_t infoBefore;
    size_t infoAfter;
    UCHAR sourceBytes[INPUT_REPORT_SIZE_CB] = { 0 };
    UCHAR outputBytes[INPUT_REPORT_SIZE_CB] = { 0 };
    WDF_REQUEST_PARAMETERS requestParameters;
    ULONG completionIoctl;
    size_t parameterOutLength;
    size_t parameterInLength;

    //
    // Probe the actual completion buffer path.
    //
    WDF_REQUEST_PARAMETERS_INIT(&requestParameters);
    WdfRequestGetParameters(Request, &requestParameters);
    completionIoctl = requestParameters.Parameters.DeviceIoControl.IoControlCode;
    parameterOutLength = requestParameters.Parameters.DeviceIoControl.OutputBufferLength;
    parameterInLength = requestParameters.Parameters.DeviceIoControl.InputBufferLength;

    status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
        "CompleteHidReadRequest req=%p retrieveOutputMemory status=%!STATUS! parameterOut=%Iu parameterIn=%Iu need=%Iu",
        Request,
        status,
        parameterOutLength,
        parameterInLength,
        NumBytes);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    destinationBuffer = WdfMemoryGetBuffer(outputMemory, &outputBufferLength);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
        "CompleteHidReadRequest req=%p outputMemoryBuffer=%p len=%Iu",
        Request,
        destinationBuffer,
        outputBufferLength);

    if (WdfRequestIsCanceled(Request)) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_HID, "CompleteHidReadRequest skipped canceled req=%p", Request);
        return STATUS_CANCELLED;
    }

    infoBefore = WdfRequestGetInformation(Request);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
        "CompleteHidReadRequest req=%p completionIoctl=0x%08X", Request, completionIoctl);

    if (outputBufferLength < NumBytes) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_HID,
            "CompleteHidReadRequest out too small cap=%Iu need=%Iu req=%p",
            outputBufferLength, NumBytes, Request);
        return STATUS_INVALID_BUFFER_SIZE;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
        "CompleteHidReadRequest req=%p source=%p outputCap=%Iu",
        Request, SourceBuffer, outputBufferLength);

    RtlZeroMemory(destinationBuffer, outputBufferLength);
    status = WdfMemoryCopyFromBuffer(outputMemory, 0, SourceBuffer, NumBytes);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_HID,
            "CompleteHidReadRequest WdfMemoryCopyFromBuffer failed req=%p status=%!STATUS!",
            Request,
            status);
        return status;
    }

    if (NumBytes >= 1) {
        RtlCopyMemory(sourceBytes, SourceBuffer, (NumBytes < sizeof(sourceBytes)) ? NumBytes : sizeof(sourceBytes));
    }
    if (outputBufferLength >= 1) {
        RtlCopyMemory(outputBytes, destinationBuffer, (outputBufferLength < sizeof(outputBytes)) ? outputBufferLength : sizeof(outputBytes));
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
        "CompleteHidReadRequest source bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X",
        sourceBytes[0], sourceBytes[1], sourceBytes[2], sourceBytes[3],
        sourceBytes[4], sourceBytes[5], sourceBytes[6], sourceBytes[7], sourceBytes[8]);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
        "CompleteHidReadRequest destination bytes (%Iu): %02X %02X %02X %02X %02X %02X %02X %02X %02X",
        outputBufferLength,
        outputBytes[0], outputBytes[1], outputBytes[2], outputBytes[3],
        outputBytes[4], outputBytes[5], outputBytes[6], outputBytes[7], outputBytes[8]);

    //
    // Important contract check for WDF completion metadata.
    // With multiple top-level collections, keyboard reports include a report ID.
    //
    infoAfter = infoBefore;
    WdfRequestSetInformation(Request, NumBytes);
    infoAfter = WdfRequestGetInformation(Request);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
        "CompleteHidReadRequest req=%p info before=%Iu after=%Iu", Request, infoBefore, infoAfter);

    return STATUS_SUCCESS;
}

static ULONG ClampMaxHoldMs(_In_ ULONG MaxHoldMs)
{
    ULONG holdMs = (MaxHoldMs == 0) ? 1500u : MaxHoldMs;
    return (holdMs > 1500u) ? 1500u : holdMs;
}

static BOOLEAN IsModifierUsage(_In_ USHORT usage)
{
    return (usage >= 0xE0u && usage <= 0xE7u) ? TRUE : FALSE;
}

static VOID MarkCommandResult(_In_ PDEVICE_CONTEXT DeviceContext, _In_ ULONG commandStatus, _In_ USHORT usage)
{
    DeviceContext->LastCommandStatus = commandStatus;
    DeviceContext->LastCommandUsage = usage;
    DeviceContext->Sequence++;
}

static NTSTATUS HandleKeyDownCommand(_In_ PDEVICE_CONTEXT DeviceContext, _In_ PTHESPIKEYDRIVER_KEY_EVENT_INPUT Input)
{
    ULONGLONG deadline = GetTickCount64() + ClampMaxHoldMs(Input->MaxHoldMs);
    UCHAR i;

    if (Input->Usage == 0) {
        MarkCommandResult(DeviceContext, THESPIKEYDRIVER_COMMAND_STATUS_INVALID_USAGE, Input->Usage);
        return STATUS_INVALID_PARAMETER;
    }

    if (IsModifierUsage(Input->Usage)) {
        UCHAR bit = (UCHAR)(Input->Usage - 0xE0u);
        UCHAR mask = (UCHAR)(1u << bit);
        if ((DeviceContext->CurrentModifier & mask) != 0) {
            DeviceContext->DuplicateNoOpCount++;
        }
        DeviceContext->CurrentModifier |= mask;
        DeviceContext->ModifierHoldDeadline[bit] = deadline;
        DeviceContext->KeyDownCount++;
        DeviceContext->KeyboardStateDirty = TRUE;
        MarkCommandResult(DeviceContext, THESPIKEYDRIVER_COMMAND_STATUS_OK, Input->Usage);
        return STATUS_SUCCESS;
    }

    for (i = 0; i < 6; ++i) {
        if (DeviceContext->CurrentKeycodes[i] == (UCHAR)Input->Usage) {
            DeviceContext->DuplicateNoOpCount++;
            DeviceContext->KeyHoldDeadline[i] = deadline;
            DeviceContext->KeyDownCount++;
            DeviceContext->KeyboardStateDirty = TRUE;
            MarkCommandResult(DeviceContext, THESPIKEYDRIVER_COMMAND_STATUS_OK, Input->Usage);
            return STATUS_SUCCESS;
        }
    }
    for (i = 0; i < 6; ++i) {
        if (DeviceContext->CurrentKeycodes[i] == 0) {
            DeviceContext->CurrentKeycodes[i] = (UCHAR)Input->Usage;
            DeviceContext->KeyHoldDeadline[i] = deadline;
            DeviceContext->KeyDownCount++;
            DeviceContext->KeyboardStateDirty = TRUE;
            MarkCommandResult(DeviceContext, THESPIKEYDRIVER_COMMAND_STATUS_OK, Input->Usage);
            return STATUS_SUCCESS;
        }
    }

    DeviceContext->RolloverRejectCount++;
    MarkCommandResult(DeviceContext, THESPIKEYDRIVER_COMMAND_STATUS_ROLLOVER_FULL, Input->Usage);
    return STATUS_BUFFER_OVERFLOW;
}

static NTSTATUS HandleKeyUpCommand(_In_ PDEVICE_CONTEXT DeviceContext, _In_ PTHESPIKEYDRIVER_KEY_EVENT_INPUT Input)
{
    UCHAR i;

    if (Input->Usage == 0) {
        MarkCommandResult(DeviceContext, THESPIKEYDRIVER_COMMAND_STATUS_INVALID_USAGE, Input->Usage);
        return STATUS_INVALID_PARAMETER;
    }

    if (IsModifierUsage(Input->Usage)) {
        UCHAR bit = (UCHAR)(Input->Usage - 0xE0u);
        UCHAR mask = (UCHAR)(1u << bit);
        if ((DeviceContext->CurrentModifier & mask) == 0) {
            DeviceContext->DuplicateNoOpCount++;
        }
        DeviceContext->CurrentModifier &= (UCHAR)~mask;
        DeviceContext->ModifierHoldDeadline[bit] = 0;
        DeviceContext->KeyUpCount++;
        DeviceContext->KeyboardStateDirty = TRUE;
        MarkCommandResult(DeviceContext, THESPIKEYDRIVER_COMMAND_STATUS_OK, Input->Usage);
        return STATUS_SUCCESS;
    }

    for (i = 0; i < 6; ++i) {
        if (DeviceContext->CurrentKeycodes[i] == (UCHAR)Input->Usage) {
            DeviceContext->CurrentKeycodes[i] = 0;
            DeviceContext->KeyHoldDeadline[i] = 0;
            DeviceContext->KeyUpCount++;
            DeviceContext->KeyboardStateDirty = TRUE;
            MarkCommandResult(DeviceContext, THESPIKEYDRIVER_COMMAND_STATUS_OK, Input->Usage);
            return STATUS_SUCCESS;
        }
    }

    DeviceContext->DuplicateNoOpCount++;
    DeviceContext->KeyUpCount++;
    MarkCommandResult(DeviceContext, THESPIKEYDRIVER_COMMAND_STATUS_OK, Input->Usage);
    return STATUS_SUCCESS;
}

static NTSTATUS HandleReleaseAllCommand(_In_ PDEVICE_CONTEXT DeviceContext)
{
    DeviceContext->CurrentModifier = 0;
    RtlZeroMemory(DeviceContext->CurrentKeycodes, sizeof(DeviceContext->CurrentKeycodes));
    RtlZeroMemory(DeviceContext->ModifierHoldDeadline, sizeof(DeviceContext->ModifierHoldDeadline));
    RtlZeroMemory(DeviceContext->KeyHoldDeadline, sizeof(DeviceContext->KeyHoldDeadline));
    DeviceContext->ReleaseAllCount++;
    DeviceContext->KeyboardStateDirty = TRUE;
    MarkCommandResult(DeviceContext, THESPIKEYDRIVER_COMMAND_STATUS_OK, 0);
    return STATUS_SUCCESS;
}

static
VOID
FillStatsCommon(
    _In_ PDEVICE_CONTEXT DeviceContext,
    _Out_ PTHESPIKEYDRIVER_STATS_OUTPUT Stats
    )
{
    RtlZeroMemory(Stats, sizeof(*Stats));
    Stats->StructSize = sizeof(*Stats);
    Stats->Version = THESPIKEYDRIVER_CONTROL_VERSION;
    Stats->Sequence = DeviceContext->Sequence;
    Stats->KeyDownCount = DeviceContext->KeyDownCount;
    Stats->KeyUpCount = DeviceContext->KeyUpCount;
    Stats->ReleaseAllCount = DeviceContext->ReleaseAllCount;
    Stats->AutoReleaseCount = DeviceContext->AutoReleaseCount;
    Stats->DuplicateNoOpCount = DeviceContext->DuplicateNoOpCount;
    Stats->RolloverRejectCount = DeviceContext->RolloverRejectCount;
    Stats->ReportsCompletedCount = DeviceContext->ReportsCompletedCount;
    Stats->EmptyTicksCount = DeviceContext->EmptyTickCount;
    Stats->LastCompletionStatus = (ULONG)DeviceContext->LastCompletionStatus;
    Stats->LastCommandStatus = DeviceContext->LastCommandStatus;
    Stats->LastCommandUsage = DeviceContext->LastCommandUsage;
    RtlCopyMemory(Stats->LastReport, DeviceContext->LastCompletedReport, sizeof(Stats->LastReport));
    Stats->CurrentModifier = DeviceContext->CurrentModifier;
    RtlCopyMemory(Stats->CurrentKeycodes, DeviceContext->CurrentKeycodes, sizeof(Stats->CurrentKeycodes));
}

static
NTSTATUS
GetFeaturePacketBuffers(
    _In_ WDFREQUEST Request,
    _Outptr_result_bytebuffer_(*InputLength) PTHESPIKEYDRIVER_FEATURE_REPORT *InputPacket,
    _Out_ size_t *InputLength,
    _Outptr_result_bytebuffer_(*OutputLength) PTHESPIKEYDRIVER_FEATURE_REPORT *OutputPacket,
    _Out_ size_t *OutputLength
    )
{
    NTSTATUS status;

    *InputPacket = NULL;
    *OutputPacket = NULL;
    *InputLength = 0;
    *OutputLength = 0;

    status = WdfRequestRetrieveInputBuffer(Request, sizeof(UCHAR), (PVOID*)InputPacket, InputLength);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = WdfRequestRetrieveOutputBuffer(Request, THESPIKEYDRIVER_FEATURE_REPORT_SIZE_CB, (PVOID*)OutputPacket, OutputLength);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    return STATUS_SUCCESS;
}

static NTSTATUS
GetStringId(
    _In_ WDFREQUEST Request,
    _Out_ ULONG *StringId,
    _Out_ ULONG *LanguageId
    )
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

NTSTATUS
GetIndexedString(
    _In_ WDFREQUEST Request
    )
{
    NTSTATUS status;
    ULONG languageId;
    ULONG stringIndex;

    status = GetStringId(Request, &stringIndex, &languageId);
    UNREFERENCED_PARAMETER(languageId);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    if (stringIndex != VHIDMINI_DEVICE_STRING_INDEX) {
        return STATUS_INVALID_PARAMETER;
    }
    return RequestCopyFromBuffer(Request, (PVOID)g_DeviceIndexed, sizeof(g_DeviceIndexed));
}

NTSTATUS
GetString(
    _In_ WDFREQUEST Request
    )
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
        stringSizeCb = sizeof(g_Product);
        string = g_Product;
        break;
    case HID_STRING_ID_ISERIALNUMBER:
        stringSizeCb = sizeof(g_Serial);
        string = g_Serial;
        break;
    default:
        return STATUS_INVALID_PARAMETER;
    }
    return RequestCopyFromBuffer(Request, (PVOID)string, stringSizeCb);
}

NTSTATUS
ReadReport(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST Request,
    _Always_(_Out_) BOOLEAN *CompleteRequest
    )
{
    NTSTATUS status;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_HID, "ReadReport forward to manual queue req=%p ioctl=0x%08X",
        Request, IOCTL_HID_READ_REPORT);
    status = WdfRequestForwardToIoQueue(Request, QueueContext->DeviceContext->ManualQueue);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_HID,
            "ReadReport forward failed req=%p status=%!STATUS!", Request, status);
        *CompleteRequest = TRUE;
        return status;
    }
    QueueContext->DeviceContext->LastForwardedReadRequest = Request;
    QueueContext->DeviceContext->LastForwardedReadIoctl = IOCTL_HID_READ_REPORT;
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_HID,
        "ReadReport recorded forwarded request=%p ioctl=0x%08X", Request, IOCTL_HID_READ_REPORT);
    *CompleteRequest = FALSE;
    return status;
}

NTSTATUS
WriteReport(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST Request
    )
{
    NTSTATUS status;
    HID_XFER_PACKET packet;

    UNREFERENCED_PARAMETER(QueueContext);

    status = RequestGetHidXferPacket_ToWriteToDevice(Request, &packet);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_HID,
            "WriteReport xfer retrieval failed status=%!STATUS! req=%p", status, Request);
        return status;
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
        "WriteReport packet req=%p reportId=%u bufferLen=%u",
        Request, packet.reportId, (ULONG)packet.reportBufferLen);

    if (packet.reportId != 0 && packet.reportId != KEYBOARD_COLLECTION_REPORT_ID) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_HID, "WriteReport bad reportId %u", packet.reportId);
        return STATUS_INVALID_PARAMETER;
    }

    if (packet.reportBufferLen < 2) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_HID, "WriteReport buffer too small %u", (ULONG)packet.reportBufferLen);
        return STATUS_INVALID_BUFFER_SIZE;
    }

    if (packet.reportBuffer[0] != KEYBOARD_COLLECTION_REPORT_ID) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_HID,
            "WriteReport bad payload reportId %u", packet.reportBuffer[0]);
        return STATUS_INVALID_PARAMETER;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID, "LED WriteReport reportId=%u byte=0x%02X",
        packet.reportBuffer[0], packet.reportBuffer[1]);

    WdfRequestSetInformation(Request, 2);
    return STATUS_SUCCESS;
}

NTSTATUS
GetFeature(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST Request
    )
{
    NTSTATUS status;
    PTHESPIKEYDRIVER_FEATURE_REPORT inputPacket;
    PTHESPIKEYDRIVER_FEATURE_REPORT outputPacket;
    size_t inputLength;
    size_t outputLength;

    status = GetFeaturePacketBuffers(Request, &inputPacket, &inputLength, &outputPacket, &outputLength);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_HID,
            "control GetFeature buffer retrieval failed status=%!STATUS! req=%p", status, Request);
        return status;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
        "control GetFeature req=%p inputLen=%Iu outputLen=%Iu reportId=%u controlCode=%u version=%u",
        Request,
        inputLength,
        outputLength,
        inputPacket->ReportId,
        (inputLength >= 2) ? inputPacket->ControlCode : 0,
        (inputLength >= 4) ? inputPacket->Version : 0);

    if (inputPacket->ReportId != CONTROL_COLLECTION_REPORT_ID) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_HID,
            "control GetFeature bad report id=%u req=%p", inputPacket->ReportId, Request);
        return STATUS_INVALID_PARAMETER;
    }
    if (inputLength >= sizeof(*inputPacket) &&
        (inputPacket->Version != THESPIKEYDRIVER_FEATURE_VERSION ||
         inputPacket->ControlCode != THESPIKEYDRIVER_FEATURE_GET_STATS)) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_HID,
            "control GetFeature bad feature size/version/control code req=%p inputLen=%Iu version=%u controlCode=%u",
            Request,
            inputLength,
            (inputLength >= 4) ? inputPacket->Version : 0,
            (inputLength >= 2) ? inputPacket->ControlCode : 0);
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(outputPacket, sizeof(*outputPacket));
    outputPacket->ReportId = CONTROL_COLLECTION_REPORT_ID;
    outputPacket->ControlCode = THESPIKEYDRIVER_FEATURE_GET_STATS;
    outputPacket->Version = THESPIKEYDRIVER_FEATURE_VERSION;
    FillStatsCommon(QueueContext->DeviceContext, &outputPacket->Payload.Stats);
    WdfRequestSetInformation(Request, sizeof(*outputPacket));
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
        "control GetFeature stats req=%p seq=%lu down=%lu up=%lu releaseAll=%lu completed=%lu",
        Request,
        outputPacket->Payload.Stats.Sequence,
        outputPacket->Payload.Stats.KeyDownCount,
        outputPacket->Payload.Stats.KeyUpCount,
        outputPacket->Payload.Stats.ReleaseAllCount,
        outputPacket->Payload.Stats.ReportsCompletedCount);
    return STATUS_SUCCESS;
}

NTSTATUS
SetFeature(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST Request
    )
{
    NTSTATUS status;
    PTHESPIKEYDRIVER_FEATURE_REPORT inputPacket;
    size_t inputLength;

    status = WdfRequestRetrieveInputBuffer(Request, sizeof(THESPIKEYDRIVER_FEATURE_REPORT), (PVOID*)&inputPacket, &inputLength);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_HID,
            "control SetFeature input retrieval failed status=%!STATUS! req=%p", status, Request);
        return status;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
        "control SetFeature req=%p inputLen=%Iu reportId=%u controlCode=%u version=%u",
        Request,
        inputLength,
        inputPacket->ReportId,
        inputPacket->ControlCode,
        inputPacket->Version);

    if (inputPacket->ReportId != CONTROL_COLLECTION_REPORT_ID) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_HID,
            "control SetFeature bad report id=%u req=%p", inputPacket->ReportId, Request);
        return STATUS_INVALID_PARAMETER;
    }
    if (inputLength < sizeof(*inputPacket) ||
        inputPacket->Version != THESPIKEYDRIVER_FEATURE_VERSION) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_HID,
            "control SetFeature bad feature size/version/control code req=%p inputLen=%Iu version=%u controlCode=%u",
            Request,
            inputLength,
            inputPacket->Version,
            inputPacket->ControlCode);
        return (inputLength < sizeof(*inputPacket)) ? STATUS_INVALID_BUFFER_SIZE : STATUS_INVALID_PARAMETER;
    }

    switch (inputPacket->ControlCode) {
    case THESPIKEYDRIVER_FEATURE_KEY_DOWN:
        status = HandleKeyDownCommand(QueueContext->DeviceContext, &inputPacket->Payload.KeyEvent);
        break;
    case THESPIKEYDRIVER_FEATURE_KEY_UP:
        status = HandleKeyUpCommand(QueueContext->DeviceContext, &inputPacket->Payload.KeyEvent);
        break;
    case THESPIKEYDRIVER_FEATURE_RELEASE_ALL:
        status = HandleReleaseAllCommand(QueueContext->DeviceContext);
        break;
    default:
        status = STATUS_INVALID_PARAMETER;
        break;
    }
    if (NT_SUCCESS(status)) {
        TheSpikeyDriverKickManualQueue(QueueContext->DeviceContext, 1);
        WdfRequestSetInformation(Request, sizeof(*inputPacket));
    }
    return status;
}

NTSTATUS
GetInputReport(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST Request,
    _Always_(_Out_) BOOLEAN *CompleteRequest
    )
{
    NTSTATUS status;
    HID_XFER_PACKET packet;

    status = RequestGetHidXferPacket_ToReadFromDevice(Request, &packet);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_HID,
            "GetInputReport xfer retrieval failed status=%!STATUS! req=%p", status, Request);
        *CompleteRequest = TRUE;
        return status;
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
        "GetInputReport packet req=%p reportId=%u bufferLen=%u expectedMin=%u",
        Request, packet.reportId, (ULONG)packet.reportBufferLen, (ULONG)sizeof(HIDMINI_KEYBOARD_INPUT_REPORT));
    if (packet.reportId != 0 && packet.reportId != KEYBOARD_COLLECTION_REPORT_ID) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_HID, "GetInputReport bad reportId %u", packet.reportId);
        *CompleteRequest = TRUE;
        return STATUS_INVALID_PARAMETER;
    }
    if (packet.reportBufferLen < sizeof(HIDMINI_KEYBOARD_INPUT_REPORT)) {
        *CompleteRequest = TRUE;
        return STATUS_INVALID_BUFFER_SIZE;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_HID, "GetInputReport forward to manual queue req=%p ioctl=0x%08X",
        Request, IOCTL_UMDF_HID_GET_INPUT_REPORT);
    status = WdfRequestForwardToIoQueue(Request, QueueContext->DeviceContext->ManualQueue);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_HID,
            "GetInputReport forward failed req=%p status=%!STATUS!", Request, status);
        *CompleteRequest = TRUE;
        return status;
    }
    QueueContext->DeviceContext->LastForwardedReadRequest = Request;
    QueueContext->DeviceContext->LastForwardedReadIoctl = IOCTL_UMDF_HID_GET_INPUT_REPORT;
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_HID,
        "GetInputReport recorded forwarded request=%p ioctl=0x%08X",
        Request, IOCTL_UMDF_HID_GET_INPUT_REPORT);
    *CompleteRequest = FALSE;
    return status;
}

NTSTATUS
SetOutputReport(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST Request
    )
{
    NTSTATUS status;
    HID_XFER_PACKET packet;

    status = RequestGetHidXferPacket_ToWriteToDevice(Request, &packet);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_HID,
            "SetOutputReport xfer retrieval failed status=%!STATUS! req=%p", status, Request);
        return status;
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
        "SetOutputReport packet req=%p reportId=%u bufferLen=%u",
        Request, packet.reportId, (ULONG)packet.reportBufferLen);
    if (packet.reportId != 0 && packet.reportId != KEYBOARD_COLLECTION_REPORT_ID) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_HID, "SetOutputReport bad reportId %u", packet.reportId);
        return STATUS_INVALID_PARAMETER;
    }
    //
    // Boot keyboard descriptor: one output byte (5 LED bits + 3 constant bits), no report ID.
    // kbdhid uses IOCTL_UMDF_HID_SET_OUTPUT_REPORT with a 1-byte payload — not phase-1 HIDMINI_OUTPUT_REPORT.
    //
    if (packet.reportBufferLen < 2) {
        return STATUS_INVALID_BUFFER_SIZE;
    }
    if (packet.reportBuffer[0] != KEYBOARD_COLLECTION_REPORT_ID) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_HID,
            "SetOutputReport bad payload reportId %u", packet.reportBuffer[0]);
        return STATUS_INVALID_PARAMETER;
    }
    QueueContext->OutputReport = packet.reportBuffer[1];
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID, "LED SetOutputReport reportId=%u byte=0x%02X",
        packet.reportBuffer[0], packet.reportBuffer[1]);
    WdfRequestSetInformation(Request, 2);
    return STATUS_SUCCESS;
}

HandleSetModeIoctl(
    _In_ PDEVICE_CONTEXT DeviceContext,
    _In_ WDFREQUEST Request
    )
{
    NTSTATUS status;
    PTHESPIKEYDRIVER_SET_MODE_INPUT input;
    size_t inputLength;

    status = WdfRequestRetrieveInputBuffer(Request, sizeof(*input), (PVOID*)&input, &inputLength);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    if (input->StructSize < sizeof(*input) || input->Version != THESPIKEYDRIVER_CONTROL_VERSION) {
        return STATUS_INVALID_PARAMETER;
    }

    UNREFERENCED_PARAMETER(DeviceContext);
    WdfRequestSetInformation(Request, 0);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID, "SetMode IOCTL accepted for compatibility mode=%lu", input->Mode);
    return STATUS_SUCCESS;
}

static
NTSTATUS
HandleGetStatsIoctl(
    _In_ PDEVICE_CONTEXT DeviceContext,
    _In_ WDFREQUEST Request
    )
{
    THESPIKEYDRIVER_STATS_OUTPUT stats;
    FillStatsCommon(DeviceContext, &stats);
    return RequestCopyFromBuffer(Request, &stats, sizeof(stats));
}

VOID
TheSpikeyDriverEvtIoHidDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    BOOLEAN completeRequest = TRUE;
    LONG totalCount;
    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    PDEVICE_CONTEXT deviceContext;
    PQUEUE_CONTEXT queueContext = QueueGetContext(Queue);

    deviceContext = DeviceGetContext(device);

    totalCount = InterlockedIncrement(&g_HidIoctlTotalCount);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
        "HID IOCTL=0x%08X (%s) req=%p in=%Iu out=%Iu total=%ld",
        IoControlCode, GetIoctlName(IoControlCode), Request, InputBufferLength, OutputBufferLength, totalCount);

    switch (IoControlCode) {
    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
        status = RequestCopyFromBuffer(Request,
            &deviceContext->HidDescriptor,
            deviceContext->HidDescriptor.bLength);
        break;
    case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
        InterlockedIncrement(&g_HidGetAttrsIoctlCount);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
            "GET_DEVICE_ATTRIBUTES returning Size=%u VID=0x%04X PID=0x%04X Ver=0x%04X",
            queueContext->DeviceContext->HidDeviceAttributes.Size,
            queueContext->DeviceContext->HidDeviceAttributes.VendorID,
            queueContext->DeviceContext->HidDeviceAttributes.ProductID,
            queueContext->DeviceContext->HidDeviceAttributes.VersionNumber);
        status = RequestCopyFromBuffer(Request,
            &queueContext->DeviceContext->HidDeviceAttributes,
            sizeof(HID_DEVICE_ATTRIBUTES));
        break;
    case IOCTL_HID_GET_REPORT_DESCRIPTOR:
    {
        InterlockedIncrement(&g_HidGetDescriptorIoctlCount);
        ULONG appCollections;
        ULONG reportIdItems;
        USHORT reportLength = deviceContext->HidDescriptor.DescriptorList[0].wReportLength;

        GetReportDescriptorStats(deviceContext->ReportDescriptor, reportLength, &appCollections, &reportIdItems);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
            "GET_REPORT_DESCRIPTOR len=%u appCollections=%u reportIdItems=%u head=%02X %02X %02X %02X %02X %02X %02X %02X",
            reportLength, appCollections, reportIdItems,
            deviceContext->ReportDescriptor[0], deviceContext->ReportDescriptor[1],
            deviceContext->ReportDescriptor[2], deviceContext->ReportDescriptor[3],
            deviceContext->ReportDescriptor[4], deviceContext->ReportDescriptor[5],
            deviceContext->ReportDescriptor[6], deviceContext->ReportDescriptor[7]);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
            "GET_REPORT_DESCRIPTOR keyboard contract hidVersion=0x%04X inputBytes=%u outputBytes=2 featureBytes=%u totalReportIdItems=%u descriptorTail=%02X %02X %02X %02X",
            deviceContext->HidDescriptor.bcdHID,
            INPUT_REPORT_SIZE_CB,
            THESPIKEYDRIVER_FEATURE_REPORT_SIZE_CB,
            reportIdItems,
            deviceContext->ReportDescriptor[reportLength - 4],
            deviceContext->ReportDescriptor[reportLength - 3],
            deviceContext->ReportDescriptor[reportLength - 2],
            deviceContext->ReportDescriptor[reportLength - 1]);
        status = RequestCopyFromBuffer(Request,
            deviceContext->ReportDescriptor,
            deviceContext->HidDescriptor.DescriptorList[0].wReportLength);
        break;
    }
    case IOCTL_HID_READ_REPORT:
        InterlockedIncrement(&g_HidReadReportIoctlCount);
        status = ReadReport(queueContext, Request, &completeRequest);
        break;
    case IOCTL_HID_WRITE_REPORT:
        InterlockedIncrement(&g_HidWriteReportIoctlCount);
        status = WriteReport(queueContext, Request);
        break;
    case IOCTL_UMDF_HID_GET_FEATURE:
        InterlockedIncrement(&g_HidGetFeatureIoctlCount);
        status = GetFeature(queueContext, Request);
        break;
    case IOCTL_UMDF_HID_SET_FEATURE:
        InterlockedIncrement(&g_HidSetFeatureIoctlCount);
        status = SetFeature(queueContext, Request);
        break;
    case IOCTL_UMDF_HID_GET_INPUT_REPORT:
        InterlockedIncrement(&g_HidGetInputReportIoctlCount);
        status = GetInputReport(queueContext, Request, &completeRequest);
        break;
    case IOCTL_UMDF_HID_SET_OUTPUT_REPORT:
        InterlockedIncrement(&g_HidSetOutputReportIoctlCount);
        status = SetOutputReport(queueContext, Request);
        break;
    case IOCTL_HID_GET_STRING:
        status = GetString(Request);
        break;
    case IOCTL_HID_GET_INDEXED_STRING:
        status = GetIndexedString(Request);
        break;
    case IOCTL_HID_ACTIVATE_DEVICE:
        //
        // vhidmini2 uses NOT_IMPLEMENTED; some class-driver paths treat failure here as
        // "device not active" and never promote reports to kbdclass even when READ_REPORT completes.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID, "IOCTL_HID_ACTIVATE_DEVICE -> SUCCESS");
        status = STATUS_SUCCESS;
        break;
    case IOCTL_HID_DEACTIVATE_DEVICE:
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID, "IOCTL_HID_DEACTIVATE_DEVICE -> SUCCESS");
        status = STATUS_SUCCESS;
        break;
    case IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST:
    case IOCTL_GET_PHYSICAL_DESCRIPTOR:
        InterlockedIncrement(&g_HidNotImplementedIoctlCount);
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_HID,
            "%s req=%p -> STATUS_NOT_IMPLEMENTED",
            GetIoctlName(IoControlCode), Request);
        status = STATUS_NOT_IMPLEMENTED;
        break;
    case IOCTL_THESPIKEYDRIVER_GET_STATS:
        status = HandleGetStatsIoctl(deviceContext, Request);
        break;
    case IOCTL_THESPIKEYDRIVER_SET_MODE:
        status = HandleSetModeIoctl(deviceContext, Request);
        break;
    default:
        InterlockedIncrement(&g_HidUnhandledIoctlCount);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_HID,
            "Unhandled IOCTL=0x%08X in=%Iu out=%Iu (completing NOT_IMPLEMENTED — kbdhid may stop posting reads)",
            IoControlCode, InputBufferLength, OutputBufferLength);
        status = STATUS_NOT_IMPLEMENTED;
        break;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
        "HID dispatch complete=%u req=%p status=%!STATUS! out=%Iu in=%Iu counters(read=%ld getIn=%ld wr=%ld setOut=%ld getFeat=%ld setFeat=%ld notImpl=%ld unhandled=%ld)",
        (UINT)completeRequest, Request, status, OutputBufferLength, InputBufferLength,
        g_HidReadReportIoctlCount, g_HidGetInputReportIoctlCount, g_HidWriteReportIoctlCount,
        g_HidSetOutputReportIoctlCount, g_HidGetFeatureIoctlCount, g_HidSetFeatureIoctlCount,
        g_HidNotImplementedIoctlCount, g_HidUnhandledIoctlCount);

    if (completeRequest) {
        WdfRequestComplete(Request, status);
    } else {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_HID,
            "IOCTL 0x%08X deferred to manual queue req=%p (expected later completion)", IoControlCode, Request);
    }
}
