/*++ Manual queue + periodic completion for ReadReport (vhidmini2). --*/

#include "driver.h"
#include "manualqueue.tmh"

#define IDLE_TIMER_PERIOD_MS 2000u
#define DIRTY_RETRY_DELAY_MS 25u

static VOID ScheduleNextReadReportTimer(_In_ PMANUAL_QUEUE_CONTEXT QueueContext, _In_ ULONG DelayMs)
{
    WdfTimerStart(QueueContext->Timer, WDF_REL_TIMEOUT_IN_MS(DelayMs));
}

VOID SCMFD_Keyboard_RootKickManualQueue(_In_ PDEVICE_CONTEXT DeviceContext, _In_ ULONG DelayMs)
{
    PMANUAL_QUEUE_CONTEXT queueContext;
    if (DeviceContext == NULL || DeviceContext->ManualQueue == NULL) {
        return;
    }
    queueContext = GetManualQueueContext(DeviceContext->ManualQueue);
    WdfTimerStart(queueContext->Timer, WDF_REL_TIMEOUT_IN_MS(DelayMs));
}

static BOOLEAN ApplyWatchdog(_In_ PDEVICE_CONTEXT DeviceContext)
{
    ULONGLONG now = GetTickCount64();
    BOOLEAN changed = FALSE;
    UCHAR index;

    for (index = 0; index < 8; ++index) {
        if (((DeviceContext->CurrentModifier >> index) & 0x01u) != 0 &&
            DeviceContext->ModifierHoldDeadline[index] != 0 &&
            now >= DeviceContext->ModifierHoldDeadline[index]) {
            DeviceContext->CurrentModifier &= (UCHAR)~(1u << index);
            DeviceContext->ModifierHoldDeadline[index] = 0;
            changed = TRUE;
        }
    }

    for (index = 0; index < 6; ++index) {
        if (DeviceContext->CurrentKeycodes[index] != 0 &&
            DeviceContext->KeyHoldDeadline[index] != 0 &&
            now >= DeviceContext->KeyHoldDeadline[index]) {
            DeviceContext->CurrentKeycodes[index] = 0;
            DeviceContext->KeyHoldDeadline[index] = 0;
            changed = TRUE;
        }
    }

    if (changed) {
        DeviceContext->AutoReleaseCount++;
        DeviceContext->LastCommandStatus = SCMFD_KEYBOARD_ROOT_COMMAND_STATUS_AUTO_RELEASED;
        DeviceContext->KeyboardStateDirty = TRUE;
    }

    return changed;
}

static BOOLEAN HasHeldKeys(_In_ PDEVICE_CONTEXT DeviceContext)
{
    UCHAR i;
    if (DeviceContext->CurrentModifier != 0) {
        return TRUE;
    }
    for (i = 0; i < 6; ++i) {
        if (DeviceContext->CurrentKeycodes[i] != 0) {
            return TRUE;
        }
    }
    return FALSE;
}

NTSTATUS SCMFD_Keyboard_RootManualQueueInitialize(_In_ WDFDEVICE Device, _Out_ WDFQUEUE *Queue)
{
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDF_OBJECT_ATTRIBUTES queueAttributes;
    WDFQUEUE queue;
    PMANUAL_QUEUE_CONTEXT queueContext;
    WDF_TIMER_CONFIG timerConfig;
    WDF_OBJECT_ATTRIBUTES timerAttributes;

    WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&queueAttributes, MANUAL_QUEUE_CONTEXT);

    status = WdfIoQueueCreate(Device, &queueConfig, &queueAttributes, &queue);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Manual WdfIoQueueCreate %!STATUS!", status);
        return status;
    }

    queueContext = GetManualQueueContext(queue);
    queueContext->Queue = queue;
    queueContext->DeviceContext = DeviceGetContext(Device);

    WDF_TIMER_CONFIG_INIT(&timerConfig, SCMFD_Keyboard_RootEvtTimerFunc);
    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
    timerAttributes.ParentObject = queue;
    status = WdfTimerCreate(&timerConfig, &timerAttributes, &queueContext->Timer);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfTimerCreate %!STATUS!", status);
        return status;
    }

    WdfTimerStart(queueContext->Timer, WDF_REL_TIMEOUT_IN_MS(250));
    *Queue = queue;
    return STATUS_SUCCESS;
}

VOID SCMFD_Keyboard_RootEvtTimerFunc(_In_ WDFTIMER Timer)
{
    NTSTATUS status;
    WDFQUEUE queue;
    PMANUAL_QUEUE_CONTEXT queueContext;
    WDFREQUEST request;
    HIDMINI_KEYBOARD_INPUT_REPORT readReport;
    PDEVICE_CONTEXT deviceContext;
    ULONG nextDelayMs;

    queue = (WDFQUEUE)WdfTimerGetParentObject(Timer);
    queueContext = GetManualQueueContext(queue);
    deviceContext = queueContext->DeviceContext;

    deviceContext->TimerTickCount++;
    ApplyWatchdog(deviceContext);
    nextDelayMs = (deviceContext->KeyboardStateDirty || HasHeldKeys(deviceContext)) ? DIRTY_RETRY_DELAY_MS : IDLE_TIMER_PERIOD_MS;

    status = WdfIoQueueRetrieveNextRequest(queueContext->Queue, &request);
    if (!NT_SUCCESS(status)) {
        deviceContext->EmptyTickCount++;
        ScheduleNextReadReportTimer(queueContext, nextDelayMs);
        return;
    }

    RtlZeroMemory(&readReport, sizeof(readReport));
    readReport.ReportId = KEYBOARD_COLLECTION_REPORT_ID;
    readReport.Keyboard.Modifier = deviceContext->CurrentModifier;
    RtlCopyMemory(readReport.Keyboard.Keycodes, deviceContext->CurrentKeycodes, sizeof(readReport.Keyboard.Keycodes));

    status = CompleteHidReadRequest(request, &readReport, sizeof(readReport));
    queueContext->LastCompletedReadRequest = request;
    deviceContext->LastForwardedReadRequest = NULL;
    deviceContext->LastForwardedReadIoctl = 0;
    if (NT_SUCCESS(status)) {
        deviceContext->ReportsCompletedCount++;
        RtlCopyMemory(deviceContext->LastCompletedReport, &readReport, sizeof(deviceContext->LastCompletedReport));
        deviceContext->LastCompletionStatus = STATUS_SUCCESS;
        deviceContext->KeyboardStateDirty = FALSE;
    } else {
        deviceContext->LastCompletionStatus = status;
    }

    WdfRequestComplete(request, status);
    ScheduleNextReadReportTimer(
        queueContext,
        (deviceContext->KeyboardStateDirty || HasHeldKeys(deviceContext)) ? DIRTY_RETRY_DELAY_MS : IDLE_TIMER_PERIOD_MS);
}
