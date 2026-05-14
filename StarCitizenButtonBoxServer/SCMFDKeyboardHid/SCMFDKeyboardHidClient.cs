using System.Buffers.Binary;
using Microsoft.Win32.SafeHandles;

namespace StarCitizenButtonBoxServer.SCMFDKeyboardHid;

internal sealed class SCMFDKeyboardHidClient : IDisposable
{
    readonly Action<string>? _log;
    readonly SafeFileHandle _handle;
    readonly string _selectedPath;

    public SCMFDKeyboardHidClient(Action<string>? log = null)
    {
        _log = log;
        (_selectedPath, _handle) = OpenSingleControlHandle();
        _log?.Invoke($"[SCMFD Keyboard] Selected control path: {_selectedPath}");
        var stats = GetStats();
        _log?.Invoke($"[SCMFD Keyboard] Protocol ready. seq={stats.Sequence} down={stats.KeyDownCount} up={stats.KeyUpCount} autoRelease={stats.AutoReleaseCount}");
    }

    public Task KeyDownAsync(ushort usage, int maxHoldMs = 0, CancellationToken cancellationToken = default)
    {
        SendKeyEvent(SCMFDKeyboardHidProtocol.FeatureKeyDown, usage, maxHoldMs);
        return Task.CompletedTask;
    }

    public Task KeyUpAsync(ushort usage, CancellationToken cancellationToken = default)
    {
        SendKeyEvent(SCMFDKeyboardHidProtocol.FeatureKeyUp, usage, 0);
        return Task.CompletedTask;
    }

    public async Task TapAsync(ushort usage, TimeSpan holdDuration, CancellationToken cancellationToken = default)
    {
        await KeyDownAsync(usage, 0, cancellationToken);
        await Task.Delay(holdDuration, cancellationToken);
        await KeyUpAsync(usage, cancellationToken);
    }

    public Task ReleaseAllAsync(CancellationToken cancellationToken = default)
    {
        var report = BuildHeader(SCMFDKeyboardHidProtocol.FeatureReleaseAll);
        SetFeature(report);
        return Task.CompletedTask;
    }

    public SCMFDKeyboardHidStats GetStats()
    {
        var report = BuildHeader(SCMFDKeyboardHidProtocol.FeatureGetStats);
        GetFeature(report);
        return ParseStats(report);
    }

    void SendKeyEvent(byte featureCode, ushort usage, int maxHoldMs)
    {
        var report = BuildHeader(featureCode);
        BinaryPrimitives.WriteUInt32LittleEndian(report.AsSpan(4, 4), 12); // StructSize
        BinaryPrimitives.WriteUInt32LittleEndian(report.AsSpan(8, 4), SCMFDKeyboardHidProtocol.ControlVersion);
        BinaryPrimitives.WriteUInt16LittleEndian(report.AsSpan(12, 2), usage);
        BinaryPrimitives.WriteUInt16LittleEndian(report.AsSpan(14, 2), 0); // Flags reserved
        BinaryPrimitives.WriteUInt32LittleEndian(report.AsSpan(16, 4), (uint)Math.Max(0, maxHoldMs));
        SetFeature(report);
    }

    static byte[] BuildHeader(byte featureCode)
    {
        var report = new byte[SCMFDKeyboardHidProtocol.FeatureReportLength];
        report[0] = SCMFDKeyboardHidProtocol.ControlCollectionReportId;
        report[1] = featureCode;
        BinaryPrimitives.WriteUInt16LittleEndian(report.AsSpan(2, 2), SCMFDKeyboardHidProtocol.FeatureVersion);
        return report;
    }

    void SetFeature(byte[] report)
    {
        if (!SCMFDKeyboardHidNative.HidD_SetFeature(_handle, report, report.Length)) {
            throw new InvalidOperationException($"SCMFD Keyboard HidD_SetFeature failed. Win32={System.Runtime.InteropServices.Marshal.GetLastWin32Error()}");
        }
    }

    void GetFeature(byte[] report)
    {
        if (!SCMFDKeyboardHidNative.HidD_GetFeature(_handle, report, report.Length)) {
            throw new InvalidOperationException($"SCMFD Keyboard HidD_GetFeature failed. Win32={System.Runtime.InteropServices.Marshal.GetLastWin32Error()}");
        }
    }

    static SCMFDKeyboardHidStats ParseStats(byte[] report)
    {
        // Payload starts at byte 4.
        return new SCMFDKeyboardHidStats
        {
            Sequence = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(12, 4)),
            KeyDownCount = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(16, 4)),
            KeyUpCount = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(20, 4)),
            ReleaseAllCount = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(24, 4)),
            AutoReleaseCount = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(28, 4)),
            DuplicateNoOpCount = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(32, 4)),
            RolloverRejectCount = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(36, 4)),
            ReportsCompletedCount = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(40, 4)),
            LastCommandStatus = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(52, 4)),
            LastCommandUsage = BinaryPrimitives.ReadUInt32LittleEndian(report.AsSpan(56, 4)),
            CurrentModifier = report[66],
            CurrentKeycodes = report.AsSpan(67, 6).ToArray()
        };
    }

    static (string Path, SafeFileHandle Handle) OpenSingleControlHandle()
    {
        SCMFDKeyboardHidNative.HidD_GetHidGuid(out var hidGuid);
        var cr = SCMFDKeyboardHidNative.CM_Get_Device_Interface_List_SizeW(out var len, ref hidGuid, null, SCMFDKeyboardHidNative.CmGetDeviceInterfaceListPresent);
        if (cr != SCMFDKeyboardHidNative.CrSuccess || len <= 1) {
            throw new InvalidOperationException("No HID interfaces found while searching for SCMFD Keyboard control collection.");
        }

        var buffer = new char[len];
        cr = SCMFDKeyboardHidNative.CM_Get_Device_Interface_ListW(ref hidGuid, null, buffer, len, SCMFDKeyboardHidNative.CmGetDeviceInterfaceListPresent);
        if (cr != SCMFDKeyboardHidNative.CrSuccess) {
            throw new InvalidOperationException($"CM_Get_Device_Interface_ListW failed with CR=0x{cr:X}");
        }

        var allCandidates = new List<string>();
        var scmfdKeyboardCandidates = new List<string>();
        int idx = 0;
        while (idx < buffer.Length && buffer[idx] != '\0') {
            int start = idx;
            while (idx < buffer.Length && buffer[idx] != '\0') idx++;
            var path = new string(buffer, start, idx - start);
            idx++;

            var handle = TryOpen(path);
            if (handle == null) continue;
            try {
                if (IsSCMFDKeyboardControlCollection(handle, out var isVendorMatch)) {
                    allCandidates.Add(path);
                    if (isVendorMatch) scmfdKeyboardCandidates.Add(path);
                }
            } finally {
                handle.Dispose();
            }
        }

        var target = scmfdKeyboardCandidates.Count > 0 ? scmfdKeyboardCandidates : allCandidates;
        if (target.Count == 0) {
            throw new InvalidOperationException("No SCMFD Keyboard control collection found.");
        }
        if (target.Count > 1) {
            throw new InvalidOperationException("Duplicate SCMFD Keyboard control collections found:\n" + string.Join("\n", target));
        }

        var finalHandle = TryOpen(target[0]) ?? throw new InvalidOperationException($"Unable to open selected SCMFD Keyboard path: {target[0]}");
        return (target[0], finalHandle);
    }

    static SafeFileHandle? TryOpen(string path)
    {
        var handle = SCMFDKeyboardHidNative.CreateFileW(
            path,
            SCMFDKeyboardHidNative.GenericRead | SCMFDKeyboardHidNative.GenericWrite,
            SCMFDKeyboardHidNative.FileShareRead | SCMFDKeyboardHidNative.FileShareWrite,
            IntPtr.Zero,
            SCMFDKeyboardHidNative.OpenExisting,
            0,
            IntPtr.Zero);
        if (!handle.IsInvalid) return handle;
        handle.Dispose();

        handle = SCMFDKeyboardHidNative.CreateFileW(
            path,
            SCMFDKeyboardHidNative.GenericRead,
            SCMFDKeyboardHidNative.FileShareRead | SCMFDKeyboardHidNative.FileShareWrite,
            IntPtr.Zero,
            SCMFDKeyboardHidNative.OpenExisting,
            0,
            IntPtr.Zero);
        if (!handle.IsInvalid) return handle;
        handle.Dispose();
        return null;
    }

    static bool IsSCMFDKeyboardControlCollection(SafeFileHandle handle, out bool vendorMatch)
    {
        vendorMatch = false;
        var attributes = new SCMFDKeyboardHidNative.HiddAttributes { Size = System.Runtime.InteropServices.Marshal.SizeOf<SCMFDKeyboardHidNative.HiddAttributes>() };
        if (!SCMFDKeyboardHidNative.HidD_GetAttributes(handle, ref attributes)) return false;
        if (!SCMFDKeyboardHidNative.HidD_GetPreparsedData(handle, out var ppd) || ppd == IntPtr.Zero) return false;
        try {
            if (SCMFDKeyboardHidNative.HidP_GetCaps(ppd, out var caps) != SCMFDKeyboardHidNative.HidpStatusSuccess) return false;
            var controlMatch = caps.UsagePage == SCMFDKeyboardHidProtocol.ControlUsagePage &&
                               caps.Usage == SCMFDKeyboardHidProtocol.ControlUsage &&
                               caps.FeatureReportByteLength == SCMFDKeyboardHidProtocol.FeatureReportLength;
            vendorMatch = attributes.VendorID == 0x5343 && attributes.ProductID == 0x4B42;
            return controlMatch;
        } finally {
            SCMFDKeyboardHidNative.HidD_FreePreparsedData(ppd);
        }
    }

    public void Dispose()
    {
        _handle.Dispose();
    }
}
