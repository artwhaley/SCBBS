using StarCitizenButtonBoxServer.SCMFDJoystickHid;

namespace SCMFDJoystickTestClient;

internal static class Program
{
    static async Task<int> Main(string[] args)
    {
        try {
            var delaySeconds = ConsumeDelay(ref args);
            if (delaySeconds > 0) {
                Console.WriteLine($"Running command in {delaySeconds} seconds. Focus the target window now.");
                await Task.Delay(TimeSpan.FromSeconds(delaySeconds));
            }

            if (args.Length == 0) {
                PrintUsage();
                return 2;
            }

            using var client = new SCMFDJoystickHidClient(Console.WriteLine);
            switch (args[0].ToLowerInvariant()) {
                case "stats":
                    PrintStats(client.GetStats());
                    return 0;

                case "button":
                    if (args.Length != 3 || !int.TryParse(args[1], out var button)) {
                        throw new ArgumentException("Usage: button <1..128> down|up");
                    }
                    var pressed = ParseButtonState(args[2]);
                    await client.SetButtonAsync(button, pressed);
                    PrintStats(client.GetStats());
                    return 0;

                case "axis":
                    if (args.Length != 3 || !int.TryParse(args[2], out var value)) {
                        throw new ArgumentException("Usage: axis x|y|z|rx|ry|rz|slider|dial <-32768..32767>");
                    }
                    await client.SetAxisAsync(ParseAxis(args[1]), value);
                    PrintStats(client.GetStats());
                    return 0;

                case "release-all":
                    await client.ReleaseAllAsync();
                    PrintStats(client.GetStats());
                    return 0;

                default:
                    PrintUsage();
                    return 2;
            }
        } catch (Exception ex) {
            Console.Error.WriteLine(ex.Message);
            return 1;
        }
    }

    static int ConsumeDelay(ref string[] args)
    {
        if (args.Length < 2 || !string.Equals(args[0], "--delay", StringComparison.OrdinalIgnoreCase)) {
            return 0;
        }

        if (!int.TryParse(args[1], out var delaySeconds) || delaySeconds < 0) {
            throw new ArgumentException("--delay must be a non-negative integer number of seconds.");
        }

        args = args.Skip(2).ToArray();
        return delaySeconds;
    }

    static bool ParseButtonState(string value)
    {
        return value.ToLowerInvariant() switch
        {
            "down" => true,
            "up" => false,
            _ => throw new ArgumentException("Button state must be 'down' or 'up'.")
        };
    }

    static SCMFDJoystickAxis ParseAxis(string value)
    {
        return value.ToLowerInvariant() switch
        {
            "x" => SCMFDJoystickAxis.X,
            "y" => SCMFDJoystickAxis.Y,
            "z" => SCMFDJoystickAxis.Z,
            "rx" => SCMFDJoystickAxis.Rx,
            "ry" => SCMFDJoystickAxis.Ry,
            "rz" => SCMFDJoystickAxis.Rz,
            "slider" => SCMFDJoystickAxis.Slider,
            "dial" => SCMFDJoystickAxis.Dial,
            _ when uint.TryParse(value, out var index) && index < SCMFDJoystickHidProtocol.AxisCount => (SCMFDJoystickAxis)index,
            _ => throw new ArgumentException("Axis must be x, y, z, rx, ry, rz, slider, dial, or index 0..7.")
        };
    }

    static void PrintStats(SCMFDJoystickHidStats stats)
    {
        Console.WriteLine(stats);
    }

    static void PrintUsage()
    {
        Console.WriteLine("SCMFDJoystickTestClient");
        Console.WriteLine("  stats");
        Console.WriteLine("  button <1..128> down|up");
        Console.WriteLine("  axis x|y|z|rx|ry|rz|slider|dial <-32768..32767>");
        Console.WriteLine("  release-all");
        Console.WriteLine("  --delay <seconds> <command>");
    }
}
