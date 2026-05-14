namespace StarCitizenButtonBoxServer.SCMFDJoystickHid;

internal static class SCMFDJoystickHidProtocol
{
    internal const ushort VendorId = 0xDEED;
    internal const ushort ProductIdLegacy = 0xF00D;
    internal const ushort ProductIdA = 0xF10A;
    internal const ushort ProductIdB = 0xF00B;

    internal const byte ControlCollectionReportId = 0x01;
    internal const byte JoystickCollectionReportId = 0x02;
    internal const ushort FeatureVersion = 1;
    internal const uint ControlVersion = 1;

    internal const byte FeatureGetStats = 0x01;
    internal const byte FeatureButton = 0x02;
    internal const byte FeatureAxis = 0x03;
    internal const byte FeatureReleaseAll = 0x04;

    internal const uint StatusOk = 0;
    internal const uint StatusInvalidParameters = 1;
    internal const uint StatusInvalidButton = 2;
    internal const uint StatusInvalidAxis = 3;

    internal const int AxisCount = 8;
    internal const int ButtonCount = 128;
    internal const int ButtonBytes = 16;
    internal const int InputReportLength = 33;
    internal const int FeatureReportLength = 118;
    internal const ushort ControlUsagePage = 0xFF00;
    internal const ushort ControlUsage = 0x0001;
}
