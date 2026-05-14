/*++ Manual queue completion for SCMFD_Joystick_B_Root. --*/

#include "driver.h"
#include "manualqueue.tmh"

#define JOYSTICK_IDLE_TIMER_PERIOD_MS 16u

static VOID ScheduleNextReadReportTimer(_In_ PMANUAL_QUEUE_CONTEXT QueueContext, _In_ ULONG DelayMs)
{
    WdfTimerStart(QueueContext->Timer, WDF_REL_TIMEOUT_IN_MS(DelayMs));
}

VOID SCMFD_Joystick_B_RootKickManualQueue(_In_ PDEVICE_CONTEXT DeviceContext, _In_ ULONG DelayMs)
{
    PMANUAL_QUEUE_CONTEXT queueContext;
    if (DeviceContext == NULL || DeviceContext->ManualQueue == NULL) {
        return;
    }
    queueContext = GetManualQueueContext(DeviceContext->ManualQueue);
    WdfTimerStart(queueContext->Timer, WDF_REL_TIMEOUT_IN_MS(DelayMs));
}

NTSTATUS SCMFD_Joystick_B_RootManualQueueInitialize(_In_ WDFDEVICE Device, _Out_ WDFQUEUE *Queue)
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

    WDF_TIMER_CONFIG_INIT(&timerConfig, SCMFD_Joystick_B_RootEvtTimerFunc);
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

VOID SCMFD_Joystick_B_RootEvtTimerFunc(_In_ WDFTIMER Timer)
{
    NTSTATUS status;
    WDFQUEUE queue;
    PMANUAL_QUEUE_CONTEXT queueContext;
    WDFREQUEST request;
    PDEVICE_CONTEXT deviceContext;
    SCMFD_JOYSTICK_ROOT_INPUT_REPORT report;

    queue = (WDFQUEUE)WdfTimerGetParentObject(Timer);
    queueContext = GetManualQueueContext(queue);
    deviceContext = queueContext->DeviceContext;

    deviceContext->TimerTickCount++;

    status = WdfIoQueueRetrieveNextRequest(queueContext->Queue, &request);
    if (!NT_SUCCESS(status)) {
        deviceContext->EmptyTickCount++;
        ScheduleNextReadReportTimer(queueContext, JOYSTICK_IDLE_TIMER_PERIOD_MS);
        return;
    }

    report = deviceContext->CurrentReport;
    report.ReportId = JOYSTICK_COLLECTION_REPORT_ID;

    status = CompleteHidReadRequest(request, &report, sizeof(report));
    queueContext->LastCompletedReadRequest = request;
    deviceContext->LastForwardedReadRequest = NULL;
    deviceContext->LastForwardedReadIoctl = 0;

    if (NT_SUCCESS(status)) {
        deviceContext->Sequence++;
        deviceContext->ReportsCompletedCount++;
        deviceContext->LastCompletedReport = report;
        deviceContext->LastCompletionStatus = STATUS_SUCCESS;
    } else {
        deviceContext->LastCompletionStatus = status;
    }

    WdfRequestComplete(request, status);
    ScheduleNextReadReportTimer(queueContext, JOYSTICK_IDLE_TIMER_PERIOD_MS);
}
