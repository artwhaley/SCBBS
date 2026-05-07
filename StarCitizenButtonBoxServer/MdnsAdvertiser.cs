using Makaretu.Dns;

namespace StarCitizenButtonBoxServer;

public sealed class MdnsAdvertiser : IDisposable
{
    static readonly string ServiceType = "_starcommander._tcp";

    readonly ServiceDiscovery _serviceDiscovery = new();
    ServiceProfile? _profile;
    bool _isRunning;
    string? _instanceName;

    public string? InstanceName => _instanceName;
    public bool IsRunning => _isRunning;

    public void Start(string instanceName, int port)
    {
        Stop();

        var profile = new ServiceProfile(instanceName, ServiceType, (ushort)port);
        _serviceDiscovery.Advertise(profile);

        _profile = profile;
        _instanceName = instanceName;
        _isRunning = true;
    }

    public void Stop()
    {
        if (!_isRunning || _profile is null)
        {
            _isRunning = false;
            return;
        }

        _serviceDiscovery.Unadvertise(_profile);
        _isRunning = false;
        _profile = null;
    }

    public void Dispose()
    {
        Stop();
        _serviceDiscovery.Dispose();
    }
}
