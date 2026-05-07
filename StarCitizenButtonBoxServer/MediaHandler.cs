using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Graphics.Imaging;
using Windows.Media.Control;
using Windows.Storage.Streams;
using NAudio.CoreAudioApi;
using WindowsInput;
using WindowsInput.Native;

namespace StarCitizenButtonBoxServer;


public sealed class MediaHandler : IDisposable
{
    GlobalSystemMediaTransportControlsSessionManager? _manager;

    public event EventHandler<MediaSnapshot>? PlaybackChanged;

    public async Task InitializeAsync()
    {
        _manager = await GlobalSystemMediaTransportControlsSessionManager.RequestAsync().AsTask();
        _manager.CurrentSessionChanged += OnCurrentSessionChanged;
        AttachSession(_manager.GetCurrentSession());
        await PublishSnapshotAsync();
    }

    void OnCurrentSessionChanged(GlobalSystemMediaTransportControlsSessionManager sender, object args) =>
        AttachSession(sender.GetCurrentSession());

    GlobalSystemMediaTransportControlsSession? _attached;

    void AttachSession(GlobalSystemMediaTransportControlsSession? session)
    {
        if (_attached != null)
        {
            _attached.MediaPropertiesChanged -= OnSessionMediaPropertiesChanged;
            _attached.PlaybackInfoChanged -= OnSessionPlaybackInfoChanged;
        }

        _attached = session;
        if (_attached != null)
        {
            _attached.MediaPropertiesChanged += OnSessionMediaPropertiesChanged;
            _attached.PlaybackInfoChanged += OnSessionPlaybackInfoChanged;
        }

        _ = PublishSnapshotAsync();
    }

    void OnSessionMediaPropertiesChanged(GlobalSystemMediaTransportControlsSession sender, object args) =>
        _ = PublishSnapshotAsync();

    void OnSessionPlaybackInfoChanged(GlobalSystemMediaTransportControlsSession sender, object args) =>
        _ = PublishSnapshotAsync();

    public async Task PublishSnapshotAsync()
    {
        try
        {
            var snap = await GetSnapshotAsync();
            PlaybackChanged?.Invoke(this, snap);
        }
        catch
        {
            PlaybackChanged?.Invoke(this, new MediaSnapshot("", "", "", null));
        }
    }

    public async Task<MediaSnapshot> GetSnapshotAsync()
    {
        var session = _manager?.GetCurrentSession();
        if (session == null)
            return new MediaSnapshot("", "", "", null);

        var props = await session.TryGetMediaPropertiesAsync().AsTask();
        var title = props.Title ?? "";
        var artist = props.Artist ?? "";
        var album = props.AlbumTitle ?? props.Subtitle ?? "";

        string? b64 = null;
        var thumb = props.Thumbnail;
        if (thumb != null)
        {
            try
            {
                var stream = await thumb.OpenReadAsync().AsTask();
                var png = await TranscodeToPngBytesAsync(stream);
                b64 = Convert.ToBase64String(png);
            }
            catch
            {
                b64 = null;
            }
        }

        return new MediaSnapshot(title, artist, album, b64);
    }

    static async Task<byte[]> TranscodeToPngBytesAsync(IRandomAccessStream input)
    {
        var decoder = await BitmapDecoder.CreateAsync(input).AsTask();
        var bmp = await decoder.GetSoftwareBitmapAsync().AsTask();
        using var outMem = new InMemoryRandomAccessStream();
        var encoder = await BitmapEncoder.CreateAsync(BitmapEncoder.PngEncoderId, outMem).AsTask();
        encoder.SetSoftwareBitmap(bmp);
        await encoder.FlushAsync().AsTask();
        outMem.Seek(0);
        using var reader = new DataReader(outMem.GetInputStreamAt(0));
        var size = outMem.Size;
        await reader.LoadAsync((uint)size).AsTask();
        var bytes = new byte[size];
        reader.ReadBytes(bytes);
        return bytes;
    }

    public async Task<bool> HandleAction(string action)
    {
        // Volume works without an active media session.
        if (action == "volume_up")
        {
            SystemVolume.Adjust(1);
            return true;
        }

        if (action == "volume_down")
        {
            SystemVolume.Adjust(-1);
            return true;
        }

        var session = _manager?.GetCurrentSession();
        if (session == null)
            return false;

        try
        {
            switch (action)
            {
                case "play_pause":
                    return await session.TryTogglePlayPauseAsync().AsTask();
                case "next_track":
                    return await session.TrySkipNextAsync().AsTask();
                case "previous_track":
                    return await session.TrySkipPreviousAsync().AsTask();
                default:
                    return false;
            }
        }
        catch
        {
            return false;
        }
    }

    public void Dispose()
    {
        if (_attached != null)
        {
            _attached.MediaPropertiesChanged -= OnSessionMediaPropertiesChanged;
            _attached.PlaybackInfoChanged -= OnSessionPlaybackInfoChanged;
            _attached = null;
        }

        if (_manager != null)
        {
            _manager.CurrentSessionChanged -= OnCurrentSessionChanged;
            _manager = null;
        }
    }
}

public sealed record MediaSnapshot(string Title, string Artist, string Album, string? ThumbnailBase64Png);


public static class SystemVolume
{
    const float Step = 0.05f;

    public static void Adjust(int direction)
    {
        try
        {
            using var enumerator = new MMDeviceEnumerator();
            var device = enumerator.GetDefaultAudioEndpoint(DataFlow.Render, Role.Multimedia);
            var vol = device.AudioEndpointVolume;
            vol.MasterVolumeLevelScalar = Math.Clamp(vol.MasterVolumeLevelScalar + (direction > 0 ? Step : -Step), 0f, 1f);
        }
        catch
        {
            // CoreAudio shrugged, so try the boring keyboard keys.
            try
            {
                var sim = new InputSimulator();
                sim.Keyboard.KeyPress(direction > 0 ? VirtualKeyCode.VOLUME_UP : VirtualKeyCode.VOLUME_DOWN);
            }
            catch { }
        }
    }
}
