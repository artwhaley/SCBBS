/*++ Device creation (vhidmini2: FDO filter, no custom device interface). --*/

#include "driver.h"
#include "device.tmh"

static volatile LONG g_SCMFD_Keyboard_RootDeviceOrdinal;

EVT_WDF_DEVICE_FILE_CREATE SCMFD_Keyboard_RootEvtDeviceFileCreate;
EVT_WDF_FILE_CLOSE SCMFD_Keyboard_RootEvtFileClose;
EVT_WDF_FILE_CLEANUP SCMFD_Keyboard_RootEvtFileCleanup;

VOID
SCMFD_Keyboard_RootEvtDeviceFileCreate(
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject
    )
{
    WDF_REQUEST_PARAMETERS requestParameters;
    PCUNICODE_STRING fileName;
    ULONG createOptions;
    ULONG fileAttributes;
    USHORT shareAccess;

    WDF_REQUEST_PARAMETERS_INIT(&requestParameters);
    WdfRequestGetParameters(Request, &requestParameters);
    createOptions = requestParameters.Parameters.Create.Options;
    fileAttributes = requestParameters.Parameters.Create.FileAttributes;
    shareAccess = requestParameters.Parameters.Create.ShareAccess;
    fileName = WdfFileObjectGetFileName(FileObject);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
        "EvtDeviceFileCreate device=%p req=%p file=%p namePtr=%p nameLen=%hu options=0x%08X attrs=0x%08X share=0x%04X",
        Device,
        Request,
        FileObject,
        (fileName != NULL) ? fileName->Buffer : NULL,
        (fileName != NULL) ? fileName->Length : 0,
        createOptions,
        fileAttributes,
        shareAccess);

    WdfRequestComplete(Request, STATUS_SUCCESS);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
        "EvtDeviceFileCreate completed req=%p status=%!STATUS! info=%Iu",
        Request,
        STATUS_SUCCESS,
        WdfRequestGetInformation(Request));
}

VOID
SCMFD_Keyboard_RootEvtFileCleanup(
    _In_ WDFFILEOBJECT FileObject
    )
{
    PCUNICODE_STRING fileName;

    fileName = WdfFileObjectGetFileName(FileObject);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
        "EvtFileCleanup file=%p namePtr=%p nameLen=%hu",
        FileObject,
        (fileName != NULL) ? fileName->Buffer : NULL,
        (fileName != NULL) ? fileName->Length : 0);
}

VOID
SCMFD_Keyboard_RootEvtFileClose(
    _In_ WDFFILEOBJECT FileObject
    )
{
    PCUNICODE_STRING fileName;

    fileName = WdfFileObjectGetFileName(FileObject);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
        "EvtFileClose file=%p namePtr=%p nameLen=%hu",
        FileObject,
        (fileName != NULL) ? fileName->Buffer : NULL,
        (fileName != NULL) ? fileName->Length : 0);
}

NTSTATUS
SCMFD_Keyboard_RootCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
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
        SCMFD_Keyboard_RootEvtDeviceFileCreate,
        SCMFD_Keyboard_RootEvtFileClose,
        SCMFD_Keyboard_RootEvtFileCleanup);
    WDF_OBJECT_ATTRIBUTES_INIT(&fileObjectAttributes);
    WdfDeviceInitSetFileObjectConfig(DeviceInit, &fileConfig, &fileObjectAttributes);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfDeviceCreate %!STATUS!", status);
        return status;
    }
    status = WdfDeviceCreateDeviceInterface(device, &GUID_DEVINTERFACE_SCMFD_KEYBOARD_ROOT_CONTROL, NULL);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfDeviceCreateDeviceInterface %!STATUS!", status);
        return status;
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
        "WdfDeviceCreateDeviceInterface succeeded device=%p controlGuid={e2590c85-380c-4d9c-818c-3a9f98eaa3d5}",
        device);

    deviceContext = DeviceGetContext(device);
    deviceContext->Device = device;
    deviceContext->DeviceData = 0;
    deviceContext->ReadReportDescFromRegistry = FALSE;
    deviceContext->DebugInstanceOrdinal = (ULONG)InterlockedIncrement(&g_SCMFD_Keyboard_RootDeviceOrdinal);
    deviceContext->LastForwardedReadRequest = NULL;
    deviceContext->LastForwardedReadIoctl = 0;
    deviceContext->Sequence = 0;
    deviceContext->KeyDownCount = 0;
    deviceContext->KeyUpCount = 0;
    deviceContext->ReleaseAllCount = 0;
    deviceContext->AutoReleaseCount = 0;
    deviceContext->DuplicateNoOpCount = 0;
    deviceContext->RolloverRejectCount = 0;
    deviceContext->TimerTickCount = 0;
    deviceContext->ReportsCompletedCount = 0;
    deviceContext->EmptyTickCount = 0;
    deviceContext->LastCompletionStatus = STATUS_SUCCESS;
    RtlZeroMemory(deviceContext->LastCompletedReport, sizeof(deviceContext->LastCompletedReport));
    deviceContext->CurrentModifier = 0;
    RtlZeroMemory(deviceContext->CurrentKeycodes, sizeof(deviceContext->CurrentKeycodes));
    RtlZeroMemory(deviceContext->ModifierHoldDeadline, sizeof(deviceContext->ModifierHoldDeadline));
    RtlZeroMemory(deviceContext->KeyHoldDeadline, sizeof(deviceContext->KeyHoldDeadline));
    deviceContext->LastCommandStatus = SCMFD_KEYBOARD_ROOT_COMMAND_STATUS_OK;
    deviceContext->LastCommandUsage = 0;
    deviceContext->KeyboardStateDirty = TRUE;
    if (deviceContext->DebugInstanceOrdinal > 1) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "More than one SCMFD_Keyboard_Root FDO in this WUDF host (ordinal=%u). "
            "Each has its own kbdhid child; duplicates often yield no visible typing — remove extra PnP nodes.",
            deviceContext->DebugInstanceOrdinal);
    }

    hidAttributes = &deviceContext->HidDeviceAttributes;
    RtlZeroMemory(hidAttributes, sizeof(HID_DEVICE_ATTRIBUTES));
    hidAttributes->Size = sizeof(HID_DEVICE_ATTRIBUTES);
    hidAttributes->VendorID = HIDMINI_VID;
    hidAttributes->ProductID = HIDMINI_PID;
    hidAttributes->VersionNumber = HIDMINI_VERSION;

    status = SCMFD_Keyboard_RootQueueInitialize(device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = SCMFD_Keyboard_RootManualQueueInitialize(device, &deviceContext->ManualQueue);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    deviceContext->DefaultQueue = WdfDeviceGetDefaultQueue(device);

    deviceContext->HidDescriptor = G_DefaultHidDescriptor;
    deviceContext->ReportDescriptor = G_DefaultReportDescriptor;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
        "Device created instanceOrdinal=%u VID=0x%x PID=0x%x (match to Get-PnpDevice *SCMFD Keyboard* rows)",
        deviceContext->DebugInstanceOrdinal, HIDMINI_VID, HIDMINI_PID);
    return STATUS_SUCCESS;
}
