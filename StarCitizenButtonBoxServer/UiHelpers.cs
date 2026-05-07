using System.Globalization;
using System.Windows.Data;

namespace StarCitizenButtonBoxServer;
public sealed class IntStringConverter : IValueConverter
{
    public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value is int i) return i.ToString(culture);
        return "";
    }

    public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value is string s && int.TryParse(s, NumberStyles.Integer, culture, out var i))
            return i;
        return Binding.DoNothing;
    }
}
