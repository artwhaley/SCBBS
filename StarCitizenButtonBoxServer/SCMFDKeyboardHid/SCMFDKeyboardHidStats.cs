namespace StarCitizenButtonBoxServer.SCMFDKeyboardHid;

internal sealed class SCMFDKeyboardHidStats
{
    public uint Sequence { get; init; }
    public uint KeyDownCount { get; init; }
    public uint KeyUpCount { get; init; }
    public uint ReleaseAllCount { get; init; }
    public uint AutoReleaseCount { get; init; }
    public uint DuplicateNoOpCount { get; init; }
    public uint RolloverRejectCount { get; init; }
    public uint ReportsCompletedCount { get; init; }
    public uint LastCommandStatus { get; init; }
    public uint LastCommandUsage { get; init; }
    public byte CurrentModifier { get; init; }
    public byte[] CurrentKeycodes { get; init; } = new byte[6];
}
