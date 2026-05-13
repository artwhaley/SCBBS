using System.Text.Json;
using System.Windows;

namespace StarCitizenButtonBoxServer;

public partial class App : System.Windows.Application
{
    public static BindingManager Bindings { get; private set; } = null!;
    public static KeyboardBackendRouter InputRouter { get; private set; } = null!;
    public static MediaHandler Media { get; private set; } = null!;
    public static ButtonBoxServer WebSocket { get; private set; } = null!;

    static Mutex? _singleInstanceMutex;

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        Wpf.Ui.Appearance.ApplicationThemeManager.ApplySystemTheme();

        _singleInstanceMutex = new Mutex(initiallyOwned: true, "StarCitizenButtonBoxServer_SingleInstance", out var createdNew);
        if (!createdNew)
        {
            MessageBox.Show("Star Citizen Button Box Server is already running.",
                "Already Running", MessageBoxButton.OK, MessageBoxImage.Information);
            Shutdown(0);
            return;
        }

        // This is the tiny elevated side-quest for adding the firewall rule.
        if (e.Args.Length >= 2 && e.Args[0] == "--setup-firewall" &&
            int.TryParse(e.Args[1], out var setupPort))
        {
            FirewallHelper.AddRule(setupPort);
            Shutdown(0);
            return;
        }

        Bindings = new BindingManager();
        _ = InitializeAsync();
    }

    async Task InitializeAsync()
    {
        try
        {
            await Bindings.LoadOrCreateCommandsAsync();
        }
        catch (JsonException ex)
        {
            MessageBox.Show(
                $"commands.json is invalid JSON:\n{ex.Message}\n\nFix the file and restart.",
                "Command config error", MessageBoxButton.OK, MessageBoxImage.Warning);
            Shutdown(1);
            return;
        }
        catch (Exception ex)
        {
            MessageBox.Show($"Could not load commands.json:\n{ex.Message}", "Command config error",
                MessageBoxButton.OK, MessageBoxImage.Warning);
            Shutdown(1);
            return;
        }

        try
        {
            await Bindings.LoadBindingsAsync();
        }
        catch (JsonException ex)
        {
            MessageBox.Show(
                $"bindings.json is invalid JSON:\n{ex.Message}\n\nFix the file and restart.",
                "Bindings config error", MessageBoxButton.OK, MessageBoxImage.Warning);
            Shutdown(1);
            return;
        }
        catch (Exception ex)
        {
            MessageBox.Show($"Could not load bindings.json:\n{ex.Message}", "Bindings config error",
                MessageBoxButton.OK, MessageBoxImage.Warning);
            Shutdown(1);
            return;
        }

        InputRouter = new KeyboardBackendRouter(line =>
            Dispatcher.BeginInvoke(() => ServerDashboardWindow.TryAppendLog(line)));

        try {
            InputRouter.Register(new InputHandler(line =>
                Dispatcher.BeginInvoke(() => ServerDashboardWindow.TryAppendLog(line))));
        } catch (Exception ex) {
            ServerDashboardWindow.TryAppendLog($"[Input] Interception unavailable: {ex.Message}");
        }

        try {
            InputRouter.Register(new SpikeyKeyboardBackend(line =>
                Dispatcher.BeginInvoke(() => ServerDashboardWindow.TryAppendLog(line))));
        } catch (Exception ex) {
            ServerDashboardWindow.TryAppendLog($"[Input] Spikey unavailable: {ex.Message}");
        }

        InputRouter.InitializeDefaultSelection();
        if (InputRouter.SelectedBackend is null) {
            MessageBox.Show(
                "No keyboard backend is available.\n\nInstall Interception and/or Spikey driver, then restart.",
                "Input backend error", MessageBoxButton.OK, MessageBoxImage.Error);
        }
        Media = new MediaHandler();
        WebSocket = new ButtonBoxServer(Bindings, InputRouter, Media);

        WebSocket.StatusLog += (_, line) =>
            Dispatcher.BeginInvoke(() => ServerDashboardWindow.TryAppendLog(line));

        Media.PlaybackChanged += (_, snap) => WebSocket.BroadcastMedia(snap);

        try
        {
            await Media.InitializeAsync();
        }
        catch (Exception ex)
        {
            var msg = $"[{DateTime.Now:HH:mm:ss}] Media transport unavailable: {ex.Message}";
            _ = Dispatcher.BeginInvoke(() => ServerDashboardWindow.TryAppendLog(msg));
        }

        FirewallHelper.EnsureRuleExists(Bindings.ServerPort);

        try
        {
            WebSocket.Start(Bindings.ServerPort);
            ServerDashboardWindow.RefreshEndpoint();
        }
        catch (Exception ex)
        {
            ServerDashboardWindow.RefreshEndpoint();
            MessageBox.Show($"Could not start WebSocket on port {Bindings.ServerPort}:\n{ex.Message}",
                "Server error", MessageBoxButton.OK, MessageBoxImage.Error);
        }

        Dispatcher.Invoke(() =>
        {
            var main = new MainWindow();
            MainWindow = main;
            main.Show();
            main.Hide();
        });
    }

    public static void RestartServer()
    {
        try
        {
            WebSocket.Stop();
            WebSocket.Start(Bindings.ServerPort);
            ServerDashboardWindow.RefreshEndpoint();
        }
        catch (Exception ex)
        {
            ServerDashboardWindow.RefreshEndpoint();
            MessageBox.Show($"Failed to restart WebSocket server:\n{ex.Message}", "Server error",
                MessageBoxButton.OK, MessageBoxImage.Warning);
        }
    }

    protected override void OnExit(ExitEventArgs e)
    {
        WebSocket?.Dispose();
        Media?.Dispose();
        try
        {
            InputRouter?.ReleaseAllAsync().GetAwaiter().GetResult();
        }
        catch
        {
            // Best effort only on shutdown.
        }
        InputRouter?.Dispose();
        _singleInstanceMutex?.ReleaseMutex();
        _singleInstanceMutex?.Dispose();
        base.OnExit(e);
    }
}
