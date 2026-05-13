namespace StarCitizenButtonBoxServer.SCMFDKeyboardHid;

internal static class SCMFDKeyboardHidProtocol
{
    internal const byte ControlCollectionReportId = 0x01;
    internal const ushort FeatureVersion = 1;
    internal const uint ControlVersion = 1;

    internal const byte FeatureGetStats = 0x01;
    internal const byte FeatureKeyDown = 0x02;
    internal const byte FeatureKeyUp = 0x03;
    internal const byte FeatureReleaseAll = 0x04;

    internal const int FeatureReportLength = 76;
    internal const ushort ControlUsagePage = 0xFF00;
    internal const ushort ControlUsage = 0x0001;
}
