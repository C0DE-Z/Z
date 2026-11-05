#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QColor>
#include <QEvent>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <optional>
#include <vector>

class QApplication;
class QWidget;

// Every colour the UI paints with. Derived roles are computed from the nine
// editable base colours of a Theme so a custom theme only needs a handful of picks.
struct ThemePalette {
    QColor canvas, surface, raised, raisedHover, line, lineStrong;
    QColor ink, inkMuted, inkSubtle, inkDisabled;
    QColor accent, accentHover, accentLight, accentSelected, accentSoft, accentSoftBorder, accentSoftHover, accentInk;
    QColor audio, audioFill;
};

struct Theme {
    QString name;
    bool builtIn = false;
    QColor canvas, surface, raised, line, lineStrong, ink, inkMuted, inkSubtle, accent;
    int cornerRadius = 3;
    int fontSize = 11;
    std::optional<ThemePalette> fixedPalette;

    ThemePalette resolve() const;
    QJsonObject toJson() const;
    static std::optional<Theme> fromJson(const QJsonObject& object);
};

// QColor that follows the active theme: colours that match one of the default
// (Z Pink) design tokens are swapped for the current theme's equivalent.
class ThemedColor : public QColor {
public:
    ThemedColor(const char* hex);
    ThemedColor(const QString& hex);
    ThemedColor(int r, int g, int b, int a = 255);
    ThemedColor(Qt::GlobalColor color) : QColor(color) {}
    ThemedColor(const QColor& color);
};

class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager& instance();

    void initialize(QApplication& app);

    QStringList themeNames() const;
    const Theme* find(const QString& name) const;
    const Theme& current() const { return currentTheme; }
    const ThemePalette& palette() const { return currentPalette; }

    bool setCurrent(const QString& name, bool persist = true);
    void preview(const Theme& theme);
    bool saveCustomTheme(const Theme& theme, QString* error = nullptr);
    bool deleteCustomTheme(const QString& name);
    bool importTheme(const QString& path, Theme* out, QString* error = nullptr);
    bool exportTheme(const Theme& theme, const QString& path, QString* error = nullptr) const;
    QString uniqueName(const QString& base) const;
    QString themesDirectory() const;

    QString remapStyleSheet(const QString& styleSheet) const;
    QColor remapColor(const QColor& color) const;

signals:
    void themeChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    ThemeManager() = default;

    void loadCustomThemes();
    void applyCurrent();
    void adoptWidget(QWidget* widget);
    QString buildStyleSheet() const;

    QApplication* application = nullptr;
    std::vector<Theme> themes;
    Theme currentTheme;
    ThemePalette currentPalette;
};

#endif // THEMEMANAGER_H
