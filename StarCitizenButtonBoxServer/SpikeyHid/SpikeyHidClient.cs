using System.Buffers.Binary;
using Microsoft.Win32.SafeHandles;

namespace StarCitizenButtonBoxServer.SpikeyHid;

internal sealed class SpikeyHidClient : IDisposable
{
    readonly Action<string>? _log;
    readonly SafeFileHandle _handle;
    readonly string _selectedPath;

    public SpikeyHidClient(Action<string>? log = null)
    {
        _log = log;
        (_selectedPath, _handle) = OpenSingleControlHandle();
        _log?.Invoke($"[Spikey] Selected control path: {_selectedPath}");
        var stats = GetStats();
        _log?.Invoke($"[Spikey] Protocol ready. seq={stats.Sequence} down={stats.KeyDownCount} up={stats.KeyUpCount} autoRelease={stats.AutoReleaseCount}");
    }

    public Task KeyDownAsync(ushort usage, int maxHoldMs = 0, CancellationToken cancellationToken = default)
    {
        SendKeyEvent(SpikeyHidProtocol.FeatureKeyDown, usage, maxHoldMs);
        return Task.CompletedTask;
    }

    public Task KeyUpAsync(ushort usage, CancellationToken cancellationToken = default)
    {
        SendKeyEvent(SpikeyHidProtocol.FeatureKeyUp, usage, 0);
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
        var report = BuildHeader(SpikeyHidProtocol.FeatureReleaseAll);
        SetFeature(report);
        return Task.CompletedTask;
    }

    public SpikeyHidStats GetStats()
    {
        var report = BuildHeader(SpikeyHidProtocol.FeatureGetStats);
        GetFeature(report);
        return ParseStats(report);
    }

    void SendKeyEvent(byte featureCode, ushort usage, int maxHoldMs)
    {
        var report = BuildHeader(featureCode);
        BinaryPrimitives.WriteUInt32LittleEndian(report.AsSpan(4, 4), 12); // StructSize
        BinaryPrimitives.WriteUInt32LittleEndian(report.AsSpan(8, 4), SpikeyHidProtocol.ControlVersion);
        BinaryPrimitives.WriteUInt16LittleEndian(report.AsSpan(12, 2), usage);
        BinaryPrimitives.WriteUInt16LittleEndian(report.AsSpan(14, 2), 0); // Flags reserved
        BinaryPrimitives.WriteUInt32LittleEndian(report.AsSpan(16, 4), (uint)Math.Max(0, maxHoldMs));
        SetFeature(report);
    }

    static byte[] BuildHeader(byte featureCode)
    {
        var report = new byte[SpikeyHidProtocol.FeatureReportLength];
        report[0] = SpikeyHidProtocol.ControlCollectionReportId;
        report[1] = featureCode;
        BinaryPrimitives.WriteUInt16LittleEndian(report.AsSpan(2, 2), SpikeyHidProtocol.FeatureVersion);
        return report;
    }

    void SetFeature(byte[] report)
    {
        if (!SpikeyHidNative.HidD_SetFeature(_handle, report, report.Length)) {
            throw new InvalidOperationException($"Spikey HidD_SetFeature failed. Win32={System.Runtime.InteropServices.Marshal.GetLastWin32Error()}");
        }
    }

    void GetFeature(byte[] report)
    {
        if (!SpikeyHidNative.HidD_GetFeature(_handle, report, report.Length)) {
            throw new InvalidOperationException($"Spikey HidD_GetFeature failed. Win32={System.Runtime.InteropServices.Marshal.GetLastWin32Error()}");
        }
    }

    static SpikeyHidStats ParseStats(byte[] report)
    {
        // Payload starts at byte 4.
        return new SpikeyHidStats
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
        SpikeyHidNative.HidD_GetHidGuid(out var hidGuid);
        var cr = SpikeyHidNative.CM_Get_Device_Interface_List_SizeW(out var len, ref hidGuid, null, SpikeyHidNative.CmGetDeviceInterfaceListPresent);
        if (cr != SpikeyHidNative.CrSuccess || len <= 1) {
            throw new InvalidOperationException("No HID interfaces found while searching for Spikey control collection.");
        }

        var buffer = new char[len];
        cr = SpikeyHidNative.CM_Get_Device_Interface_ListW(ref hidGuid, null, buffer, len, SpikeyHidNative.CmGetDeviceInterfaceListPresent);
        if (cr != SpikeyHidNative.CrSuccess) {
            throw new InvalidOperationException($"CM_Get_Device_Interface_ListW failed with CR=0x{cr:X}");
        }

        var allCandidates = new List<string>();
        var spikeyCandidates = new List<string>();
        int idx = 0;
        while (idx < buffer.Length && buffer[idx] != '\0') {
            int start = idx;
            while (idx < buffer.Length && buffer[idx] != '\0') idx++;
            var path = new string(buffer, start, idx - start);
            idx++;

            var handle = TryOpen(path);
            if (handle == null) continue;
            try {
                if (IsSpikeyControlCollection(handle, out var isVendorMatch)) {
                    allCandidates.Add(path);
                    if (isVendorMatch) spikeyCandidates.Add(path);
                }
            } finally {
                handle.Dispose();
            }
        }

        var target = spikeyCandidates.Count > 0 ? spikeyCandidates : allCandidates;
        if (target.Count == 0) {
            throw new InvalidOperationException("No Spikey control collection found.");
        }
        if (target.Count > 1) {
            throw new InvalidOperationException("Duplicate Spikey control collections found:\n" + string.Join("\n", target));
        }

        var finalHandle = TryOpen(target[0]) ?? throw new InvalidOperationException($"Unable to open selected Spikey path: {target[0]}");
        return (target[0], finalHandle);
    }

    static SafeFileHandle? TryOpen(string path)
    {
        var handle = SpikeyHidNative.CreateFileW(
            path,
            SpikeyHidNative.GenericRead | SpikeyHidNative.GenericWrite,
            SpikeyHidNative.FileShareRead | SpikeyHidNative.FileShareWrite,
            IntPtr.Zero,
            SpikeyHidNative.OpenExisting,
            0,
            IntPtr.Zero);
        if (!handle.IsInvalid) return handle;
        handle.Dispose();

        handle = SpikeyHidNative.CreateFileW(
            path,
            SpikeyHidNative.GenericRead,
            SpikeyHidNative.FileShareRead | SpikeyHidNative.FileShareWrite,
            IntPtr.Zero,
            SpikeyHidNative.OpenExisting,
            0,
            IntPtr.Zero);
        if (!handle.IsInvalid) return handle;
        handle.Dispose();
        return null;
    }

    static bool IsSpikeyControlCollection(SafeFileHandle handle, out bool vendorMatch)
    {
        vendorMatch = false;
        var attributes = new SpikeyHidNative.HiddAttributes { Size = System.Runtime.InteropServices.Marshal.SizeOf<SpikeyHidNative.HiddAttributes>() };
        if (!SpikeyHidNative.HidD_GetAttributes(handle, ref attributes)) return false;
        if (!SpikeyHidNative.HidD_GetPreparsedData(handle, out var ppd) || ppd == IntPtr.Zero) return false;
        try {
            if (SpikeyHidNative.HidP_GetCaps(ppd, out var caps) != SpikeyHidNative.HidpStatusSuccess) return false;
            var controlMatch = caps.UsagePage == SpikeyHidProtocol.ControlUsagePage &&
                               caps.Usage == SpikeyHidProtocol.ControlUsage &&
                               caps.FeatureReportByteLength == SpikeyHidProtocol.FeatureReportLength;
            vendorMatch = attributes.VendorID == 0xDEED && attributes.ProductID == 0xFEED;
            return controlMatch;
        } finally {
            SpikeyHidNative.HidD_FreePreparsedData(ppd);
        }
    }

    public void Dispose()
    {
        _handle.Dispose();
    }
}
