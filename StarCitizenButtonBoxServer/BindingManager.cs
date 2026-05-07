using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace StarCitizenButtonBoxServer;


public sealed class BindingManager
{
    static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        Converters = { new JsonStringEnumConverter(JsonNamingPolicy.CamelCase) }
    };

    public string CommandsPath { get; }
    public string BindingsPath { get; }

    CommandConfigRoot _commands = new();
    BindingConfigRoot _bindings = new();

    public IReadOnlyList<CommandCategory> Categories => _commands.Categories;

    public int ServerPort
    {
        get => _bindings.ServerPort;
        set => _bindings.ServerPort = Math.Clamp(value, 1, 65535);
    }

    public BindingManager(string? baseDir = null)
    {
        baseDir ??= AppContext.BaseDirectory;
        CommandsPath = Path.Combine(baseDir, "commands.json");
        BindingsPath = Path.Combine(baseDir, "bindings.json");
    }

    public async Task LoadOrCreateCommandsAsync()
    {
        if (!File.Exists(CommandsPath))
            throw new FileNotFoundException(
                $"Missing required command configuration file: {CommandsPath}",
                CommandsPath);

        await using var stream = File.OpenRead(CommandsPath);
        _commands = await JsonSerializer.DeserializeAsync<CommandConfigRoot>(stream, JsonOptions)
                    ?? new CommandConfigRoot();
    }

    public async Task LoadBindingsAsync()
    {
        if (!File.Exists(BindingsPath))
            throw new FileNotFoundException(
                $"Missing required bindings configuration file: {BindingsPath}",
                BindingsPath);

        await using var stream = File.OpenRead(BindingsPath);
        _bindings = await JsonSerializer.DeserializeAsync<BindingConfigRoot>(stream, JsonOptions)
                    ?? new BindingConfigRoot();
    }

    public async Task SaveBindingsAsync()
    {
        await using var stream = File.Create(BindingsPath);
        await JsonSerializer.SerializeAsync(stream, _bindings, JsonOptions);
    }


    public BindingEntry? GetBinding(string commandId)
    {
        return _bindings.Bindings.FirstOrDefault(b => b.CommandId == commandId);
    }

    public void UpsertBinding(BindingEntry entry)
    {
        var i = _bindings.Bindings.FindIndex(b => b.CommandId == entry.CommandId);
        if (i >= 0)
            _bindings.Bindings[i] = entry;
        else
            _bindings.Bindings.Add(entry);
    }

    public void RemoveBinding(string commandId)
    {
        _bindings.Bindings.RemoveAll(b => b.CommandId == commandId);
    }
}

public sealed class BindingConfigRoot
{
    [JsonPropertyName("serverPort")]
    public int ServerPort { get; set; } = 8795;

    [JsonPropertyName("bindings")]
    public List<BindingEntry> Bindings { get; set; } = new();
}

public sealed class BindingEntry
{
    [JsonPropertyName("commandId")]
    public string CommandId { get; set; } = "";

    [JsonPropertyName("key")]
    public string? Key { get; set; }

    [JsonPropertyName("holdTimeMs")]
    public int HoldTimeMs { get; set; } = 25;

    [JsonPropertyName("lCtrl")]
    public bool LeftCtrl { get; set; }

    [JsonPropertyName("rCtrl")]
    public bool RightCtrl { get; set; }

    [JsonPropertyName("lShift")]
    public bool LeftShift { get; set; }

    [JsonPropertyName("rShift")]
    public bool RightShift { get; set; }

    [JsonPropertyName("lAlt")]
    public bool LeftAlt { get; set; }

    [JsonPropertyName("rAlt")]
    public bool RightAlt { get; set; }

    [JsonPropertyName("mouseAction")]
    public MouseInputAction MouseInputAction { get; set; } = MouseInputAction.None;
}

public sealed class CommandConfigRoot
{
    [JsonPropertyName("categories")]
    public List<CommandCategory> Categories { get; set; } = new();
}

public sealed class CommandCategory
{
    [JsonPropertyName("id")]
    public string Id { get; set; } = "";

    [JsonPropertyName("label")]
    public string Label { get; set; } = "";

    [JsonPropertyName("commands")]
    public List<CommandItem> Commands { get; set; } = new();
}

public sealed class CommandItem
{
    [JsonPropertyName("id")]
    public string Id { get; set; } = "";

    [JsonPropertyName("label")]
    public string Label { get; set; } = "";
}
