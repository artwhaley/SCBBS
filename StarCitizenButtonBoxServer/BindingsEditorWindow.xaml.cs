using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using Wpf.Ui.Controls;

namespace StarCitizenButtonBoxServer;

public sealed class CategoryNode : INotifyPropertyChanged
{
    bool _expanded = true;
    public string Id { get; init; } = "";
    public string Label { get; init; } = "";
    public ObservableCollection<CommandItem> Commands { get; } = new();
    public bool IsExpanded
    {
        get => _expanded;
        set { _expanded = value; OnPropertyChanged(); }
    }
    public event PropertyChangedEventHandler? PropertyChanged;
    void OnPropertyChanged([CallerMemberName] string? n = null) =>
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(n));
}

public sealed class EditorState : INotifyPropertyChanged
{
    public ObservableCollection<CategoryNode> Categories { get; } = new();

    string _selectedLabel = "Select a command from the tree";
    public string SelectedCommandLabel { get => _selectedLabel; set { _selectedLabel = value; Notify(); } }

    string _keyDisplay = "";
    public string KeyDisplay { get => _keyDisplay; set { _keyDisplay = value; Notify(); Notify(nameof(KeyFieldDisplayText)); Notify(nameof(IsKeyboardInputActive)); } }

    MouseInputAction _mouseAction = MouseInputAction.None;
    public MouseInputAction MouseAction
    {
        get => _mouseAction;
        set
        {
            _mouseAction = value;
            Notify(); Notify(nameof(IsMouseInputActionSet)); Notify(nameof(IsKeyboardInputActive));
            Notify(nameof(MouseKeyFieldLabel)); Notify(nameof(KeyFieldDisplayText));
            Notify(nameof(IsMouseLeft)); Notify(nameof(IsMouseMiddle)); Notify(nameof(IsMouseRight));
            Notify(nameof(IsMouseScrollUp)); Notify(nameof(IsMouseScrollDown));
        }
    }

    public string KeyFieldDisplayText => _mouseAction != MouseInputAction.None ? MouseKeyFieldLabel : _keyDisplay;
    public bool IsMouseInputActionSet => _mouseAction != MouseInputAction.None;
    public bool IsKeyboardInputActive => _mouseAction == MouseInputAction.None;
    public string MouseKeyFieldLabel => _mouseAction switch
    {
        MouseInputAction.LeftClick => "[LMB]",
        MouseInputAction.MiddleClick => "[MMB]",
        MouseInputAction.RightClick => "[RMB]",
        MouseInputAction.ScrollUp => "[Scroll Up]",
        MouseInputAction.ScrollDown => "[Scroll Down]",
        _ => ""
    };
    public bool IsMouseLeft => _mouseAction == MouseInputAction.LeftClick;
    public bool IsMouseMiddle => _mouseAction == MouseInputAction.MiddleClick;
    public bool IsMouseRight => _mouseAction == MouseInputAction.RightClick;
    public bool IsMouseScrollUp => _mouseAction == MouseInputAction.ScrollUp;
    public bool IsMouseScrollDown => _mouseAction == MouseInputAction.ScrollDown;

    int _holdMs = 25;
    bool _lCtrl, _rCtrl, _lShift, _rShift, _lAlt, _rAlt;
    public int HoldTimeMs { get => _holdMs; set { _holdMs = value; Notify(); } }
    public bool LeftCtrl { get => _lCtrl; set { _lCtrl = value; Notify(); } }
    public bool RightCtrl { get => _rCtrl; set { _rCtrl = value; Notify(); } }
    public bool LeftShift { get => _lShift; set { _lShift = value; Notify(); } }
    public bool RightShift { get => _rShift; set { _rShift = value; Notify(); } }
    public bool LeftAlt { get => _lAlt; set { _lAlt = value; Notify(); } }
    public bool RightAlt { get => _rAlt; set { _rAlt = value; Notify(); } }

    public event PropertyChangedEventHandler? PropertyChanged;
    void Notify([CallerMemberName] string? n = null) => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(n));
}

public partial class BindingsEditorWindow : FluentWindow
{
    static BindingsEditorWindow? _instance;
    readonly EditorState _state;
    CommandItem? _selectedCommand;
    bool _suppressAutoSave;

    BindingsEditorWindow()
    {
        InitializeComponent();

        _state = new EditorState();
        DataContext = _state;

        RefreshCategories();
        Closed += (_, _) => _instance = null;

        _state.PropertyChanged += (_, e) =>
        {
            if (_suppressAutoSave || _selectedCommand == null) return;
            var saveable = e.PropertyName is nameof(EditorState.KeyDisplay) or nameof(EditorState.HoldTimeMs)
                or nameof(EditorState.LeftCtrl) or nameof(EditorState.RightCtrl)
                or nameof(EditorState.LeftShift) or nameof(EditorState.RightShift)
                or nameof(EditorState.LeftAlt) or nameof(EditorState.RightAlt)
                or nameof(EditorState.MouseAction);
            if (saveable) _ = SaveCurrentBinding();
        };
    }

    void RefreshCategories()
    {
        _state.Categories.Clear();
        foreach (var cat in App.Bindings.Categories)
        {
            var node = new CategoryNode { Id = cat.Id, Label = cat.Label };
            foreach (var cmd in cat.Commands) node.Commands.Add(cmd);
            _state.Categories.Add(node);
        }
    }

    void LoadBindingForSelection()
    {
        if (_selectedCommand == null) return;
        _suppressAutoSave = true;
        try
        {
            var b = App.Bindings.GetBinding(_selectedCommand.Id);
            if (b == null)
            {
                _state.KeyDisplay = "";
                _state.HoldTimeMs = 25;
                _state.LeftCtrl = _state.RightCtrl = _state.LeftShift = _state.RightShift = _state.LeftAlt = _state.RightAlt = false;
                _state.MouseAction = MouseInputAction.None;
                return;
            }
            _state.MouseAction = b.MouseInputAction;
            _state.KeyDisplay = b.MouseInputAction == MouseInputAction.None ? (b.Key ?? "") : "";
            _state.HoldTimeMs = Math.Clamp(b.HoldTimeMs, 1, 1000);
            _state.LeftCtrl = b.LeftCtrl; _state.RightCtrl = b.RightCtrl;
            _state.LeftShift = b.LeftShift; _state.RightShift = b.RightShift;
            _state.LeftAlt = b.LeftAlt; _state.RightAlt = b.RightAlt;
        }
        finally { _suppressAutoSave = false; }
    }

    async Task SaveCurrentBinding()
    {
        if (_selectedCommand == null) return;
        var entry = new BindingEntry
        {
            CommandId = _selectedCommand.Id,
            Key = string.IsNullOrWhiteSpace(_state.KeyDisplay) ? null : _state.KeyDisplay.Trim(),
            MouseInputAction = _state.MouseAction,
            HoldTimeMs = Math.Clamp(_state.HoldTimeMs, 1, 1000),
            LeftCtrl = _state.LeftCtrl, RightCtrl = _state.RightCtrl,
            LeftShift = _state.LeftShift, RightShift = _state.RightShift,
            LeftAlt = _state.LeftAlt, RightAlt = _state.RightAlt
        };
        App.Bindings.UpsertBinding(entry);
        await App.Bindings.SaveBindingsAsync();
        App.RestartServer();
    }

    async Task ClearCurrentBinding()
    {
        if (_selectedCommand == null) return;
        App.Bindings.RemoveBinding(_selectedCommand.Id);
        LoadBindingForSelection();
        await App.Bindings.SaveBindingsAsync();
        App.RestartServer();
    }

    public static void ShowSingleton()
    {
        if (_instance == null)
            _instance = new BindingsEditorWindow();
        _instance.RefreshCategories();
        if (!_instance.IsVisible)
            _instance.Show();
        _instance.Activate();
    }

    public static void CloseSingleton()
    {
        _instance?.Close();
    }

    void CommandTree_OnSelectedItemChanged(object sender, RoutedPropertyChangedEventArgs<object> e)
    {
        if (e.NewValue is CommandItem cmd)
        {
            _selectedCommand = cmd;
            _state.SelectedCommandLabel = cmd.Label;
            LoadBindingForSelection();
        }
        else
        {
            _selectedCommand = null;
            _state.SelectedCommandLabel = "Select a command from the tree";
        }
    }

    void MouseInputActionButton_Click(object sender, RoutedEventArgs e)
    {
        if (sender is not FrameworkElement { Tag: string tag }) return;
        if (!Enum.TryParse<MouseInputAction>(tag, out var action)) return;
        _state.MouseAction = _state.MouseAction == action ? MouseInputAction.None : action;
    }

    void KeyInput_OnPreviewKeyDown(object sender, System.Windows.Input.KeyEventArgs e)
    {
        e.Handled = true;
        var key = e.Key == Key.System ? e.SystemKey : e.Key;
        if (key is Key.LeftCtrl or Key.RightCtrl or Key.LeftShift or Key.RightShift or Key.LeftAlt or Key.RightAlt)
            return;

        if (VirtualKeyHelper.TryResolveVirtualKey(key, out var vk))
        {
            _state.MouseAction = MouseInputAction.None;
            _state.KeyDisplay = vk.ToString();
        }
    }

    async void Clear_Click(object sender, RoutedEventArgs e) => await ClearCurrentBinding();
}
