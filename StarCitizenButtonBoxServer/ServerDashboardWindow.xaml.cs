using System.Windows;
using Wpf.Ui.Controls;

namespace StarCitizenButtonBoxServer;

public partial class ServerDashboardWindow : FluentWindow
{
    static ServerDashboardWindow? _instance;
    static readonly object LogLock = new();
    static readonly List<string> PendingLog = new();
    static bool _wsClientsHooked;

    bool _logVisible;
    bool _suppressBackendToggleEvent;
    const double LogExpansionHeight = 268;

    public ServerDashboardWindow()
    {
        InitializeComponent();
        Loaded += (_, _) =>
        {
            TryFlushPendingLog();
            RefreshEndpointCore();
            RefreshClients();
            RefreshBackendControls();
        };
        Closed += (_, _) => _instance = null;
    }

    static void OnWsClientsChanged(object? sender, EventArgs e) =>
        System.Windows.Application.Current?.Dispatcher.BeginInvoke(new Action(() => RefreshClients()));

    public static void ShowSingleton()
    {
        if (_instance == null)
        {
            _instance = new ServerDashboardWindow();
            if (!_wsClientsHooked)
            {
                App.WebSocket.ClientsChanged += OnWsClientsChanged;
                _wsClientsHooked = true;
            }
        }

        _instance.Show();
        _instance.Activate();
        _instance.RefreshEndpointCore();
        _instance.RefreshBackendControls();
        RefreshClients();
    }

    public static void CloseSingleton()
    {
        _instance?.Close();
    }

    public static void TryAppendLog(string line)
    {
        var app = System.Windows.Application.Current;
        if (app == null) return;
        if (!app.Dispatcher.CheckAccess()) { app.Dispatcher.BeginInvoke(() => TryAppendLog(line)); return; }

        if (_instance != null)
        {
            _instance.LogBox.AppendText(line + Environment.NewLine);
            _instance.LogBox.CaretIndex = _instance.LogBox.Text.Length;
            _instance.LogBox.ScrollToEnd();
            return;
        }

        lock (LogLock)
        {
            PendingLog.Add(line);
            while (PendingLog.Count > 50)
                PendingLog.RemoveAt(0);
        }
    }

    void TryFlushPendingLog()
    {
        List<string> copy;
        lock (LogLock)
        {
            copy = PendingLog.ToList();
            PendingLog.Clear();
        }

        foreach (var line in copy)
        {
            LogBox.AppendText(line + Environment.NewLine);
            LogBox.CaretIndex = LogBox.Text.Length;
            LogBox.ScrollToEnd();
        }
    }

    public static void RefreshClients()
    {
        if (_instance != null)
            _instance.RefreshClientsCore();
    }

    public static void RefreshEndpoint()
    {
        if (_instance != null)
            _instance.RefreshEndpointCore();
    }

    void RefreshClientsCore()
    {
        var list = App.WebSocket.GetClientSummaries().Select((ip, i) => $"{i + 1}. {ip}").ToList();
        ClientsList.ItemsSource = list;
        TxtClientCount.Text = $"{list.Count} connected client(s)";
    }

    void RefreshEndpointCore()
    {
        TxtEndpoint.Text = $"{ButtonBoxServer.GetLocalIPv4()}:{App.Bindings.ServerPort}";
    }

    void Bindings_Click(object sender, RoutedEventArgs e) => BindingsEditorWindow.ShowSingleton();

    void ToggleLog_Click(object sender, RoutedEventArgs e)
    {
        if (!_logVisible)
        {
            _logVisible = true;
            LogAreaBorder.Visibility = Visibility.Visible;
            LogRowDefinition.Height = new GridLength(LogExpansionHeight);
            if (WindowState != WindowState.Maximized)
                Height += LogExpansionHeight;
            BtnLog.Content = "Hide Log";
        }
        else
        {
            if (WindowState != WindowState.Maximized)
                Height = Math.Max(MinHeight, Height - LogExpansionHeight);
            LogRowDefinition.Height = new GridLength(0);
            LogAreaBorder.Visibility = Visibility.Collapsed;
            BtnLog.Content = "Show Log";
            _logVisible = false;
        }
    }

    void BackendToggle_Changed(object sender, RoutedEventArgs e)
    {
        if (_suppressBackendToggleEvent) {
            return;
        }
        if (App.InputRouter == null) {
            return;
        }
        var desired = ChkUseSCMFDKeyboard.IsChecked == true ? KeyboardBackendType.SCMFDKeyboard : KeyboardBackendType.Interception;
        if (!App.InputRouter.TrySelect(desired, out var reason)) {
            TryAppendLog($"[Input] {reason}");
        } else {
            TryAppendLog($"[Input] {reason}");
        }
        RefreshBackendControls();
    }

    void RefreshBackendControls()
    {
        if (App.InputRouter == null) {
            ChkUseSCMFDKeyboard.IsEnabled = false;
            TxtBackendStatus.Text = "Input router not initialized.";
            return;
        }

        _suppressBackendToggleEvent = true;
        try {
            ChkUseSCMFDKeyboard.IsEnabled = App.InputRouter.CanSwitch;
            ChkUseSCMFDKeyboard.IsChecked = App.InputRouter.SelectedBackend == KeyboardBackendType.SCMFDKeyboard;
        } finally {
            _suppressBackendToggleEvent = false;
        }

        var selected = App.InputRouter.SelectedBackend?.ToString() ?? "None";
        var availability = $"Interception={(App.InputRouter.HasInterception ? "yes" : "no")}, SCMFD Keyboard={(App.InputRouter.HasSCMFDKeyboard ? "yes" : "no")}";
        TxtBackendStatus.Text = $"Selected: {selected} | {availability}";
    }
}
