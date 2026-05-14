/*++ Device creation for SCMFD_Joystick_Root. --*/

#include "driver.h"
#include <devpkey.h>
#include "device.tmh"

static volatile LONG g_SCMFD_Joystick_RootDeviceOrdinal;

#define SCMFD_JOYSTICK_SLOT_A 0u
#define SCMFD_JOYSTICK_SLOT_B 1u

static const WCHAR g_ProductA[] = L"SCMFD Joystick A";
static const WCHAR g_ProductB[] = L"SCMFD Joystick B";
static const WCHAR g_SerialA[] = L"SCMFD Joystick A 0001";
static const WCHAR g_SerialB[] = L"SCMFD Joystick B 0001";

// Per-slot ContainerIDs so WINMM treats A and B as distinct physical units.
// Without this, root-enumerated devices share the null container
// {00000000-0000-0000-FFFF-FFFFFFFFFFFF} and WINMM's joystick mapper
// only allocates one joyID for the pair — the second arrival is dropped.
// {F00A0DED-DEED-DEED-DEED-DEEDDEEDDEED} / {F00B0DED-...}
static const GUID g_ContainerIdA = {
    0xF00A0DED, 0xDEED, 0xDEED, { 0xDE, 0xED, 0xDE, 0xED, 0xDE, 0xED, 0xDE, 0xED }
};
static const GUID g_ContainerIdB = {
    0xF00B0DED, 0xDEED, 0xDEED, { 0xDE, 0xED, 0xDE, 0xED, 0xDE, 0xED, 0xDE, 0xED }
};

static VOID ConfigureJoystickIdentity(_In_ WDFDEVICE Device, _Inout_ PDEVICE_CONTEXT DeviceContext)
{
    NTSTATUS status;
    WDFMEMORY hardwareIdMemory = NULL;
    size_t hardwareIdLength = 0;
    PCWSTR hardwareIds;

    DeviceContext->JoystickSlot = SCMFD_JOYSTICK_SLOT_A;
    DeviceContext->ProductId = SCMFD_JOYSTICK_ROOT_PID_A;
    DeviceContext->ProductString = g_ProductA;
    DeviceContext->SerialString = g_SerialA;
    DeviceContext->IndexedString = g_ProductA;

    status = WdfDeviceAllocAndQueryProperty(
        Device,
        DevicePropertyHardwareID,
        NonPagedPoolNx,
        WDF_NO_OBJECT_ATTRIBUTES,
        &hardwareIdMemory);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
            "Could not query hardware IDs for joystick identity; defaulting to A: %!STATUS!", status);
        return;
    }

    hardwareIds = (PCWSTR)WdfMemoryGetBuffer(hardwareIdMemory, &hardwareIdLength);
    if (hardwareIds != NULL && hardwareIdLength >= sizeof(WCHAR)) {
        if (wcsstr(hardwareIds, L"SCMFD_Joystick_B") != NULL) {
            DeviceContext->JoystickSlot = SCMFD_JOYSTICK_SLOT_B;
            DeviceContext->ProductId = SCMFD_JOYSTICK_ROOT_PID_B;
            DeviceContext->ProductString = g_ProductB;
            DeviceContext->SerialString = g_SerialB;
            DeviceContext->IndexedString = g_ProductB;
        }
    }

    WdfObjectDelete(hardwareIdMemory);
}

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
    ConfigureJoystickIdentity(device, deviceContext);

    {
        WDF_DEVICE_PROPERTY_DATA containerIdProp;
        const GUID *containerId =
            (deviceContext->JoystickSlot == SCMFD_JOYSTICK_SLOT_B) ? &g_ContainerIdB : &g_ContainerIdA;
        WDF_DEVICE_PROPERTY_DATA_INIT(&containerIdProp, &DEVPKEY_Device_ContainerId);
        status = WdfDeviceAssignProperty(device, &containerIdProp, DEVPROP_TYPE_GUID, sizeof(GUID), (PVOID)containerId);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
                "WdfDeviceAssignProperty(ContainerId) slot=%u status=%!STATUS!",
                deviceContext->JoystickSlot, status);
        } else {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                "Assigned per-slot ContainerId for JoystickSlot=%u", deviceContext->JoystickSlot);
        }
    }

    deviceContext->LastCompletionStatus = STATUS_SUCCESS;
    deviceContext->LastCommandStatus = SCMFD_JOYSTICK_ROOT_STATUS_OK;
    deviceContext->CurrentReport.ReportId = JOYSTICK_COLLECTION_REPORT_ID;
    deviceContext->LastCompletedReport.ReportId = JOYSTICK_COLLECTION_REPORT_ID;

    hidAttributes = &deviceContext->HidDeviceAttributes;
    RtlZeroMemory(hidAttributes, sizeof(HID_DEVICE_ATTRIBUTES));
    hidAttributes->Size = sizeof(HID_DEVICE_ATTRIBUTES);
    hidAttributes->VendorID = HIDMINI_VID;
    hidAttributes->ProductID = deviceContext->ProductId;
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
        "SCMFD Joystick Root created ordinal=%u slot=%u VID=0x%x PID=0x%x reportSize=%u",
        deviceContext->DebugInstanceOrdinal,
        deviceContext->JoystickSlot,
        HIDMINI_VID,
        deviceContext->ProductId,
        SCMFD_JOYSTICK_ROOT_INPUT_REPORT_SIZE_CB);

    return STATUS_SUCCESS;
}
