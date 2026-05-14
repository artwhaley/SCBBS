/*++ Device creation for SCMFD_Joystick_A_Root. --*/

#include "driver.h"
#include "device.tmh"

static volatile LONG g_SCMFD_Joystick_A_RootDeviceOrdinal;

static const WCHAR g_Product[] = L"SCMFD Joystick A";
static const WCHAR g_Serial[] = L"SCMFD Joystick A 0001";

EVT_WDF_DEVICE_FILE_CREATE SCMFD_Joystick_A_RootEvtDeviceFileCreate;
EVT_WDF_FILE_CLOSE SCMFD_Joystick_A_RootEvtFileClose;
EVT_WDF_FILE_CLEANUP SCMFD_Joystick_A_RootEvtFileCleanup;

VOID SCMFD_Joystick_A_RootEvtDeviceFileCreate(
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(FileObject);
    WdfRequestComplete(Request, STATUS_SUCCESS);
}

VOID SCMFD_Joystick_A_RootEvtFileCleanup(_In_ WDFFILEOBJECT FileObject)
{
    UNREFERENCED_PARAMETER(FileObject);
}

VOID SCMFD_Joystick_A_RootEvtFileClose(_In_ WDFFILEOBJECT FileObject)
{
    UNREFERENCED_PARAMETER(FileObject);
}

NTSTATUS SCMFD_Joystick_A_RootCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit)
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
        SCMFD_Joystick_A_RootEvtDeviceFileCreate,
        SCMFD_Joystick_A_RootEvtFileClose,
        SCMFD_Joystick_A_RootEvtFileCleanup);
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
    deviceContext->DebugInstanceOrdinal = (ULONG)InterlockedIncrement(&g_SCMFD_Joystick_A_RootDeviceOrdinal);
    deviceContext->JoystickSlot = 0u;
    deviceContext->ProductId = SCMFD_JOYSTICK_ROOT_PID_A;
    deviceContext->ProductString = g_Product;
    deviceContext->SerialString = g_Serial;
    deviceContext->IndexedString = g_Product;
    deviceContext->LastCompletionStatus = STATUS_SUCCESS;
    deviceContext->LastCommandStatus = SCMFD_JOYSTICK_ROOT_STATUS_OK;
    deviceContext->CurrentReport.ReportId = JOYSTICK_COLLECTION_REPORT_ID;
    deviceContext->LastCompletedReport.ReportId = JOYSTICK_COLLECTION_REPORT_ID;

    hidAttributes = &deviceContext->HidDeviceAttributes;
    RtlZeroMemory(hidAttributes, sizeof(HID_DEVICE_ATTRIBUTES));
    hidAttributes->Size = sizeof(HID_DEVICE_ATTRIBUTES);
    hidAttributes->VendorID = HIDMINI_VID;
    hidAttributes->ProductID = SCMFD_JOYSTICK_ROOT_PID_A;
    hidAttributes->VersionNumber = HIDMINI_VERSION;

    status = SCMFD_Joystick_A_RootQueueInitialize(device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = SCMFD_Joystick_A_RootManualQueueInitialize(device, &deviceContext->ManualQueue);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    deviceContext->DefaultQueue = WdfDeviceGetDefaultQueue(device);
    deviceContext->HidDescriptor = G_DefaultHidDescriptor;
    deviceContext->ReportDescriptor = G_DefaultReportDescriptor;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
        "SCMFD Joystick A created ordinal=%u VID=0x%x PID=0x%x reportSize=%u",
        deviceContext->DebugInstanceOrdinal,
        HIDMINI_VID,
        SCMFD_JOYSTICK_ROOT_PID_A,
        SCMFD_JOYSTICK_ROOT_INPUT_REPORT_SIZE_CB);

    return STATUS_SUCCESS;
}
