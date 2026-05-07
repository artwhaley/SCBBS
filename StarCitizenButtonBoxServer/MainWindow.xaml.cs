using System.Drawing;
using System.Windows;

namespace StarCitizenButtonBoxServer;

public partial class MainWindow : Window
{
    public MainWindow()
    {
        InitializeComponent();
    }

    void OpenServer_Click(object sender, RoutedEventArgs e) => ServerDashboardWindow.ShowSingleton();

    void Tray_DoubleClick(object sender, RoutedEventArgs e) => ServerDashboardWindow.ShowSingleton();

    void Exit_Click(object sender, RoutedEventArgs e)
    {
        ServerDashboardWindow.CloseSingleton();
        BindingsEditorWindow.CloseSingleton();
        System.Windows.Application.Current.Shutdown();
    }

    void Tray_TrayContextMenuOpen(object sender, RoutedEventArgs e)
    {
        var clients = App.WebSocket is { } ws ? ws.GetClientSummaries().Count : 0;
        var port = App.Bindings.ServerPort;
        Tray.ToolTipText = $"Button Box Server: Running - {clients} client(s) - port {port}";
    }
}
