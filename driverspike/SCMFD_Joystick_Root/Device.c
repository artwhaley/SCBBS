/*++ Device creation for SCMFD_Joystick_Root. --*/

#include "driver.h"
#include "device.tmh"

static volatile LONG g_SCMFD_Joystick_RootDeviceOrdinal;

EVT_WDF_DEVICE_FILE_CREATE SCMFD_Joystick_RootEvtDeviceFileCreate;
EVT_WDF_FILE_CLOSE SCMFD_Joystick_RootEvtFileClose;
EVT_WDF_FILE_CLEANUP SCMFD_Joystick_RootEvtFileCleanup;

VOID SCMFD_Joystick_RootEvtDeviceFileCreate(
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(FileObject);
    WdfRequestComplete(Request, STATUS_SUCCESS);
}

VOID SCMFD_Joystick_RootEvtFileCleanup(_In_ WDFFILEOBJECT FileObject)
{
    UNREFERENCED_PARAMETER(FileObject);
}

VOID SCMFD_Joystick_RootEvtFileClose(_In_ WDFFILEOBJECT FileObject)
{
    UNREFERENCED_PARAMETER(FileObject);
}

NTSTATUS SCMFD_Joystick_RootCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit)
{
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    PDEVICE_CONTEXT deviceContext;
    PHID_DEVICE_ATTRIBUTES hidAttributes;
    WDFDEVICE device;
    NTSTATUS status;
    WDF_FILEOBJECT_CONFIG fileConfig;
    WDF_OBJECT_ATTRIBUTES fileObjectAttributes;

    WdfFdoInitSetFilter(DeviceInit);
    WDF_FILEOBJECT_CONFIG_INIT(
        &fileConfig,
        SCMFD_Joystick_RootEvtDeviceFileCreate,
        SCMFD_Joystick_RootEvtFileClose,
        SCMFD_Joystick_RootEvtFileCleanup);
    WDF_OBJECT_ATTRIBUTES_INIT(&fileObjectAttributes);
    WdfDeviceInitSetFileObjectConfig(DeviceInit, &fileConfig, &fileObjectAttributes);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfDeviceCreate %!STATUS!", status);
        return status;
    }

    status = WdfDeviceCreateDeviceInterface(device, &GUID_DEVINTERFACE_SCMFD_JOYSTICK_ROOT_CONTROL, NULL);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfDeviceCreateDeviceInterface %!STATUS!", status);
        return status;
    }

    deviceContext = DeviceGetContext(device);
    RtlZeroMemory(deviceContext, sizeof(*deviceContext));
    deviceContext->Device = device;
    deviceContext->ReadReportDescFromRegistry = FALSE;
    deviceContext->DebugInstanceOrdinal = (ULONG)InterlockedIncrement(&g_SCMFD_Joystick_RootDeviceOrdinal);
    deviceContext->LastCompletionStatus = STATUS_SUCCESS;
    deviceContext->LastCommandStatus = SCMFD_JOYSTICK_ROOT_STATUS_OK;
    deviceContext->CurrentReport.ReportId = JOYSTICK_COLLECTION_REPORT_ID;
    deviceContext->LastCompletedReport.ReportId = JOYSTICK_COLLECTION_REPORT_ID;

    hidAttributes = &deviceContext->HidDeviceAttributes;
    RtlZeroMemory(hidAttributes, sizeof(HID_DEVICE_ATTRIBUTES));
    hidAttributes->Size = sizeof(HID_DEVICE_ATTRIBUTES);
    hidAttributes->VendorID = HIDMINI_VID;
    hidAttributes->ProductID = HIDMINI_PID;
    hidAttributes->VersionNumber = HIDMINI_VERSION;

    status = SCMFD_Joystick_RootQueueInitialize(device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = SCMFD_Joystick_RootManualQueueInitialize(device, &deviceContext->ManualQueue);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    deviceContext->DefaultQueue = WdfDeviceGetDefaultQueue(device);
    deviceContext->HidDescriptor = G_DefaultHidDescriptor;
    deviceContext->ReportDescriptor = G_DefaultReportDescriptor;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
        "SCMFD Joystick Root created ordinal=%u VID=0x%x PID=0x%x reportSize=%u",
        deviceContext->DebugInstanceOrdinal,
        HIDMINI_VID,
        HIDMINI_PID,
        SCMFD_JOYSTICK_ROOT_INPUT_REPORT_SIZE_CB);

    return STATUS_SUCCESS;
}
