namespace StarCitizenButtonBoxServer;

public enum KeyboardBackendType
{
    Interception,
    Spikey
}

public interface IKeyboardInputBackend : IDisposable
{
    KeyboardBackendType BackendType { get; }
    Task ExecuteAsync(BindingEntry binding, CancellationToken cancellationToken = default);
    Task ReleaseAllAsync(CancellationToken cancellationToken = default);
}

public sealed class KeyboardBackendRouter : IDisposable
{
    readonly Dictionary<KeyboardBackendType, IKeyboardInputBackend> _backends = new();
    readonly Action<string>? _log;

    public KeyboardBackendType? SelectedBackend { get; private set; }
    public bool HasInterception => _backends.ContainsKey(KeyboardBackendType.Interception);
    public bool HasSpikey => _backends.ContainsKey(KeyboardBackendType.Spikey);
    public bool CanSwitch => HasInterception && HasSpikey;

    public event EventHandler? BackendSelectionChanged;

    public KeyboardBackendRouter(Action<string>? log = null)
    {
        _log = log;
    }

    public void Register(IKeyboardInputBackend backend)
    {
        _backends[backend.BackendType] = backend;
        _log?.Invoke($"[Input] Registered backend: {backend.BackendType}");
    }

    public void InitializeDefaultSelection()
    {
        if (HasInterception) {
            SelectedBackend = KeyboardBackendType.Interception;
        } else if (HasSpikey) {
            SelectedBackend = KeyboardBackendType.Spikey;
        } else {
            SelectedBackend = null;
        }
        BackendSelectionChanged?.Invoke(this, EventArgs.Empty);
    }

    public bool TrySelect(KeyboardBackendType backendType, out string reason)
    {
        if (!_backends.ContainsKey(backendType)) {
            reason = $"{backendType} backend is not available.";
            return false;
        }
        SelectedBackend = backendType;
        reason = $"Selected backend: {backendType}";
        _log?.Invoke($"[Input] {reason}");
        BackendSelectionChanged?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public async Task ExecuteAsync(BindingEntry binding, CancellationToken cancellationToken = default)
    {
        if (SelectedBackend == null || !_backends.TryGetValue(SelectedBackend.Value, out var backend)) {
            throw new InvalidOperationException("No keyboard backend is available.");
        }
        await backend.ExecuteAsync(binding, cancellationToken);
    }

    public async Task ReleaseAllAsync(CancellationToken cancellationToken = default)
    {
        foreach (var backend in _backends.Values) {
            try {
                await backend.ReleaseAllAsync(cancellationToken);
            } catch {
                // Best effort on shutdown.
            }
        }
    }

    public void Dispose()
    {
        foreach (var backend in _backends.Values) {
            backend.Dispose();
        }
    }
}
