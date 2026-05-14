using System.Buffers.Binary;

namespace StarCitizenButtonBoxServer.SCMFDJoystickHid;

internal sealed class SCMFDJoystickReport
{
    public byte ReportId { get; init; }
    public short[] Axes { get; init; } = new short[SCMFDJoystickHidProtocol.AxisCount];
    public byte[] ButtonBytes { get; init; } = new byte[SCMFDJoystickHidProtocol.ButtonBytes];

    public bool IsButtonPressed(int oneBasedButton)
    {
        if (oneBasedButton < 1 || oneBasedButton > SCMFDJoystickHidProtocol.ButtonCount) {
            throw new ArgumentOutOfRangeException(nameof(oneBasedButton), "Button must be in the range 1..128.");
        }

        var zeroBased = oneBasedButton - 1;
        return (ButtonBytes[zeroBased / 8] & (1 << (zeroBased % 8))) != 0;
    }

    internal static SCMFDJoystickReport Parse(ReadOnlySpan<byte> bytes)
    {
        if (bytes.Length < SCMFDJoystickHidProtocol.InputReportLength) {
            throw new ArgumentException("Joystick input report is shorter than expected.", nameof(bytes));
        }

        var axes = new short[SCMFDJoystickHidProtocol.AxisCount];
        for (var i = 0; i < axes.Length; i++) {
            axes[i] = BinaryPrimitives.ReadInt16LittleEndian(bytes.Slice(1 + (i * 2), 2));
        }

        return new SCMFDJoystickReport
        {
            ReportId = bytes[0],
            Axes = axes,
            ButtonBytes = bytes.Slice(17, SCMFDJoystickHidProtocol.ButtonBytes).ToArray()
        };
    }

    public override string ToString()
    {
        return $"ReportId=0x{ReportId:X2} Axes=[{string.Join(", ", Axes)}] Buttons={FormatButtons()}";
    }

    string FormatButtons()
    {
        var pressed = new List<int>();
        for (var button = 1; button <= SCMFDJoystickHidProtocol.ButtonCount; button++) {
            if (IsButtonPressed(button)) pressed.Add(button);
        }

        return pressed.Count == 0 ? "none" : string.Join(",", pressed);
    }
}
