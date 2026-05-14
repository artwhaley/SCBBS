namespace StarCitizenButtonBoxServer.SCMFDJoystickHid;

internal sealed class SCMFDJoystickHidStats
{
    public uint Sequence { get; init; }
    public uint ButtonCommandCount { get; init; }
    public uint AxisCommandCount { get; init; }
    public uint ReleaseAllCount { get; init; }
    public uint ReportsCompletedCount { get; init; }
    public uint EmptyTicksCount { get; init; }
    public uint LastCompletionStatus { get; init; }
    public uint LastCommandStatus { get; init; }
    public uint LastButton { get; init; }
    public uint LastAxis { get; init; }
    public SCMFDJoystickReport LastReport { get; init; } = new();
    public SCMFDJoystickReport CurrentReport { get; init; } = new();

    public override string ToString()
    {
        return $"seq={Sequence} buttons={ButtonCommandCount} axes={AxisCommandCount} releaseAll={ReleaseAllCount} " +
               $"completed={ReportsCompletedCount} empty={EmptyTicksCount} lastCompletion=0x{LastCompletionStatus:X8} " +
               $"lastStatus={LastCommandStatus} lastButton={LastButton} lastAxis={LastAxis}\n" +
               $"Current: {CurrentReport}";
    }
}
