using System.Buffers.Binary;
using Microsoft.Win32.SafeHandles;

namespace StarCitizenButtonBoxServer.SCMFDJoystickHid;

internal sealed class SCMFDJoystickHidClient : IDisposable
{
    readonly Action<string>? _log;
    readonly SafeFileHandle _handle;
    readonly string _selectedPath;
    readonly SCMFDJoystickSlot _slot;

    public SCMFDJoystickHidClient(Action<string>? log = null, SCMFDJoystickSlot slot = SCMFDJoystickSlot.A)
    {
        _log = log;
        _slot = slot;
        (_selectedPath, _handle) = OpenSingleControlHandle(_slot);
        _log?.Invoke($"[SCMFD Joystick {_slot}] Selected control path: {_selectedPath}");
        var stats = GetStats();
        _log?.Invoke($"[SCMFD Joystick {_slot}] Protocol ready. seq={stats.Sequence} buttons={stats.ButtonCommandCount} axes={stats.AxisCommandCount}");
    }

    public Task SetButtonAsync(int oneBasedButton, bool pressed, CancellationToken cancellationToken = default)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(oneBasedButton, 1);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(oneBasedButton, SCMFDJoystickHidProtocol.ButtonCount);

        var report = BuildHeader(SCMFDJoystickHidProtocol.FeatureButton);
        BinaryPrimitives.WriteUInt32LittleEndian(report.AsSpan(4, 4), 16); // StructSize
        BinaryPrimitives.WriteUInt32LittleEndian(report.AsSpan(8, 4), SCMFDJoystickHidProtocol.ControlVersion);
        BinaryPrimitives.WriteUInt32LittleEndian(report.AsSpan(12, 4), (uint)oneBasedButton);
        BinaryPrimitives.WriteUInt32LittleEndian(report.AsSpan(16, 4), pressed ? 1u : 0u);
        SetFeature(report);
        return Task.CompletedTask;
    }

    public Task SetAxisAsync(SCMFDJoystickAxis axis, int value, CancellationToken cancellationToken = default)
    {
        if ((uint)axis >= SCMFDJoystickHidProtocol.AxisCount) {
            throw new ArgumentOutOfRangeException(nameof(axis), "Axis must be X, Y, Z, Rx, Ry, Rz, Slider, or Dial.");
        }
        ArgumentOutOfRangeException.ThrowIfLessThan(value, short.MinValue);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(value, short.MaxValue);

        var report = BuildHeader(SCMFDJoystickHidProtocol.FeatureAxis);
        BinaryPrimitives.WriteUInt32LittleEndian(report.AsSpan(4, 4), 16); // StructSize
        BinaryPrimitives.WriteUInt32LittleEndian(report.AsSpan(8, 4), SCMFDJoystickHidProtocol.ControlVersion);
        BinaryPrimitives.WriteUInt32LittleEndian(report.AsSpan(12, 4), (uint)axis);
        BinaryPrimitives.WriteInt32LittleEndian(report.AsSpan(16, 4), value);
        SetFeature(report);
        return Task.CompletedTask;
    }

    public Task ReleaseAllAsync(CancellationToken cancellationToken = default)
    {
        var report = BuildHeader(SCMFDJoystickHidProtocol.FeatureReleaseAll);
        SetFeature(report);
        return Task.CompletedTask;
    }

    public SCMFDJoystickHidStats GetStats()
    {
        var report = BuildHeader(SCMFDJoystickHidProtocol.FeatureGetStats);
        GetFeature(report);
        return ParseStats(report);
    }

    static byte[] BuildHeader(byte featureCode)
    {
        var report = new byte[SCMFDJoystickHidProtocol.FeatureReportLength];
        report[0] = SCMFDJoystickHidProtocol.ControlCollectionReportId;
        report[1] = featureCode;
        BinaryPrimitives.WriteUInt16LittleEndian(report.AsSpan(2, 2), SCMFDJoystickHidProtocol.FeatureVersion);
        return report;
    }

    void SetFeature(byte[] report)
    {
        if (!SCMFDJoystickHidNative.HidD_SetFeature(_handle, report, report.Length)) {
            throw new InvalidOperationException($"SCMFD Joystick HidD_SetFeature failed. Win32={System.Runtime.InteropServices.Marshal.GetLastWin32Error()}");
        }
    }

    void GetFeature(byte[] report)
    {
        if (!SCMFDJoystickHidNative.HidD_GetFeature(_handle, report, report.Length)) {
            throw new InvalidOperationException($"SCMFD Joystick HidD_GetFeature failed. Win32={System.Runtime.InteropServices.Marshal.GetLastWin32Error()}");
        }
    }

    static SCMFDJoystickHidStats ParseStats(byte[] report)
    {
        return new SCMFDJoystickHidStats
        {
            Sequence = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(12, 4)),
            ButtonCommandCount = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(16, 4)),
            AxisCommandCount = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(20, 4)),
            ReleaseAllCount = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(24, 4)),
            ReportsCompletedCount = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(28, 4)),
            EmptyTicksCount = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(32, 4)),
            LastCompletionStatus = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(36, 4)),
            LastCommandStatus = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(40, 4)),
            LastButton = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(44, 4)),
            LastAxis = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(48, 4)),
            LastReport = SCMFDJoystickReport.Parse(report.AsSpan(52, SCMFDJoystickHidProtocol.InputReportLength)),
            CurrentReport = SCMFDJoystickReport.Parse(report.AsSpan(85, SCMFDJoystickHidProtocol.InputReportLength))
        };
    }

    static (string Path, SafeFileHandle Handle) OpenSingleControlHandle(SCMFDJoystickSlot slot)
    {
        SCMFDJoystickHidNative.HidD_GetHidGuid(out var hidGuid);
        var cr = SCMFDJoystickHidNative.CM_Get_Device_Interface_List_SizeW(out var len, ref hidGuid, null, SCMFDJoystickHidNative.CmGetDeviceInterfaceListPresent);
        if (cr != SCMFDJoystickHidNative.CrSuccess || len <= 1) {
            throw new InvalidOperationException("No HID interfaces found while searching for SCMFD Joystick control collection.");
        }

        var buffer = new char[len];
        cr = SCMFDJoystickHidNative.CM_Get_Device_Interface_ListW(ref hidGuid, null, buffer, len, SCMFDJoystickHidNative.CmGetDeviceInterfaceListPresent);
        if (cr != SCMFDJoystickHidNative.CrSuccess) {
            throw new InvalidOperationException($"CM_Get_Device_Interface_ListW failed with CR=0x{cr:X}");
        }

        var candidates = new List<string>();
        int idx = 0;
        while (idx < buffer.Length && buffer[idx] != '\0') {
            int start = idx;
            while (idx < buffer.Length && buffer[idx] != '\0') idx++;
            var path = new string(buffer, start, idx - start);
            idx++;

            var handle = TryOpen(path);
            if (handle == null) continue;
            try {
                if (IsSCMFDJoystickControlCollection(handle, GetProductId(slot))) {
                    candidates.Add(path);
                }
            } finally {
                handle.Dispose();
            }
        }

        if (candidates.Count == 0) {
            throw new InvalidOperationException($"No SCMFD Joystick {slot} control collection found.");
        }
        if (candidates.Count > 1) {
            throw new InvalidOperationException($"Duplicate SCMFD Joystick {slot} control collections found:\n" + string.Join("\n", candidates));
        }

        var finalHandle = TryOpen(candidates[0]) ?? throw new InvalidOperationException($"Unable to open selected SCMFD Joystick path: {candidates[0]}");
        return (candidates[0], finalHandle);
    }

    static SafeFileHandle? TryOpen(string path)
    {
        var handle = SCMFDJoystickHidNative.CreateFileW(
            path,
            SCMFDJoystickHidNative.GenericRead | SCMFDJoystickHidNative.GenericWrite,
            SCMFDJoystickHidNative.FileShareRead | SCMFDJoystickHidNative.FileShareWrite,
            IntPtr.Zero,
            SCMFDJoystickHidNative.OpenExisting,
            0,
            IntPtr.Zero);
        if (!handle.IsInvalid) return handle;
        handle.Dispose();

        handle = SCMFDJoystickHidNative.CreateFileW(
            path,
            SCMFDJoystickHidNative.GenericRead,
            SCMFDJoystickHidNative.FileShareRead | SCMFDJoystickHidNative.FileShareWrite,
            IntPtr.Zero,
            SCMFDJoystickHidNative.OpenExisting,
            0,
            IntPtr.Zero);
        if (!handle.IsInvalid) return handle;
        handle.Dispose();
        return null;
    }

    static ushort GetProductId(SCMFDJoystickSlot slot)
    {
        return slot switch
        {
            SCMFDJoystickSlot.A => SCMFDJoystickHidProtocol.ProductIdA,
            SCMFDJoystickSlot.B => SCMFDJoystickHidProtocol.ProductIdB,
            SCMFDJoystickSlot.Legacy => SCMFDJoystickHidProtocol.ProductIdLegacy,
            _ => throw new ArgumentOutOfRangeException(nameof(slot), slot, null)
        };
    }

    static bool IsSCMFDJoystickControlCollection(SafeFileHandle handle, ushort productId)
    {
        var attributes = new SCMFDJoystickHidNative.HiddAttributes { Size = System.Runtime.InteropServices.Marshal.SizeOf<SCMFDJoystickHidNative.HiddAttributes>() };
        if (!SCMFDJoystickHidNative.HidD_GetAttributes(handle, ref attributes)) return false;
        if (attributes.VendorID != SCMFDJoystickHidProtocol.VendorId || attributes.ProductID != productId) return false;
        if (!SCMFDJoystickHidNative.HidD_GetPreparsedData(handle, out var ppd) || ppd == IntPtr.Zero) return false;
        try {
            if (SCMFDJoystickHidNative.HidP_GetCaps(ppd, out var caps) != SCMFDJoystickHidNative.HidpStatusSuccess) return false;
            return caps.UsagePage == SCMFDJoystickHidProtocol.ControlUsagePage &&
                   caps.Usage == SCMFDJoystickHidProtocol.ControlUsage &&
                   caps.FeatureReportByteLength == SCMFDJoystickHidProtocol.FeatureReportLength;
        } finally {
            SCMFDJoystickHidNative.HidD_FreePreparsedData(ppd);
        }
    }

    public void Dispose()
    {
        _handle.Dispose();
    }
}
