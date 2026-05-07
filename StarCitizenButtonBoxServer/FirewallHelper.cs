using System.Diagnostics;
using System.Windows;

namespace StarCitizenButtonBoxServer;


internal static class FirewallHelper
{
    const string RuleName = "Star Commander MFD Server";

    // Windows Firewall, because apparently "same LAN" was too generous.
    internal static void EnsureRuleExists(int port)
    {
        if (RuleExists())
            return;

        var result = MessageBox.Show(
            $"Star Commander MFD Server needs a Windows Firewall inbound rule on TCP port {port} " +
            "so tablets on your network can connect.\n\n" +
            "Click OK to approve a one-time administrator prompt to add this rule automatically.\n" +
            "Click Cancel to skip -- tablet connections will fail until the rule is added manually.",
            "Firewall Setup Required",
            MessageBoxButton.OKCancel,
            MessageBoxImage.Information);

        if (result != MessageBoxResult.OK)
            return;

        try
        {
            var exe = Environment.ProcessPath
                ?? System.Diagnostics.Process.GetCurrentProcess().MainModule!.FileName;

            using var proc = Process.Start(new ProcessStartInfo(exe, $"--setup-firewall {port}")
            {
                Verb = "runas",          // triggers UAC elevation
                UseShellExecute = true,  // required for Verb = runas
            });
            proc?.WaitForExit();
        }
        catch
        {
            // User cancelled the UAC prompt -- connections from outside this machine just won't work.
        }
    }

    // The elevated half of the firewall dance.
    internal static void AddRule(int port)
    {
        try
        {
            using var proc = Process.Start(new ProcessStartInfo(
                "netsh",
                $"advfirewall firewall add rule " +
                $"name=\"{RuleName}\" protocol=TCP dir=in localport={port} action=allow")
            {
                UseShellExecute = false,
                CreateNoWindow = true,
            });
            proc?.WaitForExit();
        }
        catch
        {
            // Best effort -- if netsh fails we just don't have the rule.
        }
    }

    static bool RuleExists()
    {
        try
        {
            using var proc = Process.Start(new ProcessStartInfo(
                "netsh",
                $"advfirewall firewall show rule name=\"{RuleName}\"")
            {
                RedirectStandardOutput = true,
                UseShellExecute = false,
                CreateNoWindow = true,
            });
            var output = proc?.StandardOutput.ReadToEnd() ?? string.Empty;
            proc?.WaitForExit();
            return output.Contains(RuleName, StringComparison.OrdinalIgnoreCase);
        }
        catch
        {
            return true; // Can't check -- don't prompt, avoid a false-positive UAC nag.
        }
    }
}
