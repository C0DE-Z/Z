#include "ui/thememanager.h"

#include <algorithm>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QPalette>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QWidget>

namespace {

constexpr auto kDefaultThemeName = "Z Pink";
constexpr const char* kAppliedProperty = "_zThemeApplied";
constexpr const char* kBaseProperty = "_zThemeBase";

QColor mix(const QColor& from, const QColor& to, double amount) {
    return QColor::fromRgbF(
        from.redF() + (to.redF() - from.redF()) * amount,
        from.greenF() + (to.greenF() - from.greenF()) * amount,
        from.blueF() + (to.blueF() - from.blueF()) * amount);
}

double luminance(const QColor& c) {
    return 0.299 * c.redF() + 0.587 * c.greenF() + 0.114 * c.blueF();
}

using PaletteMember = QColor ThemePalette::*;

// Colours baked into the original (Z Pink) UI code, keyed to the role they play.
const QHash<QRgb, PaletteMember>& defaultTokens() {
    static const QHash<QRgb, PaletteMember> tokens = [] {
        QHash<QRgb, PaletteMember> t;
        auto add = [&t](const char* hex, PaletteMember member) { t.insert(QColor(hex).rgb(), member); };
        add("#08080A", &ThemePalette::canvas);
        add("#111116", &ThemePalette::surface);
        add("#19191F", &ThemePalette::raised);
        add("#24242C", &ThemePalette::raisedHover);
        add("#303036", &ThemePalette::line);
        add("#4E4E58", &ThemePalette::lineStrong);
        add("#F7F4F6", &ThemePalette::ink);
        add("#DCDCEB", &ThemePalette::ink);
        add("#C3BEC3", &ThemePalette::inkMuted);
        add("#918B92", &ThemePalette::inkSubtle);
        add("#888898", &ThemePalette::inkSubtle);
        add("#9090A0", &ThemePalette::inkSubtle);
        add("#A096B4", &ThemePalette::inkSubtle);
        add("#706B72", &ThemePalette::inkDisabled);
        add("#6E647D", &ThemePalette::inkDisabled);
        add("#FF4F91", &ThemePalette::accent);
        add("#FF72AA", &ThemePalette::accentHover);
        add("#FFB8D2", &ThemePalette::accentLight);
        add("#F59EF8", &ThemePalette::accentLight);
        add("#3D1226", &ThemePalette::accentSelected);
        add("#32101F", &ThemePalette::accentSoft);
        add("#80324F", &ThemePalette::accentSoftBorder);
        add("#5C1E38", &ThemePalette::accentSoftHover);
        add("#2A0715", &ThemePalette::accentInk);
        add("#2DBEBF", &ThemePalette::audio);
        add("#10333A", &ThemePalette::audioFill);
        return t;
    }();
    return tokens;
}

ThemePalette zPinkPalette() {
    ThemePalette p;
    p.canvas = "#08080A"; p.surface = "#111116"; p.raised = "#19191F"; p.raisedHover = "#24242C";
    p.line = "#303036"; p.lineStrong = "#4E4E58";
    p.ink = "#F7F4F6"; p.inkMuted = "#C3BEC3"; p.inkSubtle = "#918B92"; p.inkDisabled = "#706B72";
    p.accent = "#FF4F91"; p.accentHover = "#FF72AA"; p.accentLight = "#FFB8D2";
    p.accentSelected = "#3D1226"; p.accentSoft = "#32101F"; p.accentSoftBorder = "#80324F";
    p.accentSoftHover = "#5C1E38"; p.accentInk = "#2A0715";
    p.audio = "#2DBEBF"; p.audioFill = "#10333A";
    return p;
}

Theme makeTheme(const char* name, const char* canvas, const char* surface, const char* raised, const char* line,
                const char* lineStrong, const char* ink, const char* inkMuted, const char* inkSubtle,
                const char* accent, int radius) {
    Theme t;
    t.name = name;
    t.builtIn = true;
    t.canvas = canvas; t.surface = surface; t.raised = raised; t.line = line; t.lineStrong = lineStrong;
    t.ink = ink; t.inkMuted = inkMuted; t.inkSubtle = inkSubtle; t.accent = accent;
    t.cornerRadius = radius;
    return t;
}

std::vector<Theme> builtInThemes() {
    std::vector<Theme> list;
    Theme pink = makeTheme(kDefaultThemeName, "#08080A", "#111116", "#19191F", "#303036", "#4E4E58",
                           "#F7F4F6", "#C3BEC3", "#918B92", "#FF4F91", 4);
    pink.fixedPalette = zPinkPalette();
    list.push_back(pink);
    list.push_back(makeTheme("Adobe Dark", "#1B1B1B", "#232323", "#323232", "#121212", "#4D4D4D",
                             "#F0F0F0", "#C4C4C4", "#8C8C8C", "#2D8CEB", 2));
    list.push_back(makeTheme("Adobe Darkest", "#0E0E0E", "#181818", "#262626", "#050505", "#404040",
                             "#E8E8E8", "#B5B5B5", "#7E7E7E", "#2D8CEB", 2));
    list.push_back(makeTheme("Graphite", "#1F1F23", "#2A2A2E", "#38383E", "#151518", "#55555C",
                             "#EDEDED", "#BEBEC2", "#8A8A90", "#E8A33D", 2));
    list.push_back(makeTheme("Final Cut", "#1A1A1C", "#262628", "#3A3A3D", "#0C0C0D", "#5A5A5E",
                             "#F2F2F2", "#C8C8CC", "#8E8E93", "#5E8BFF", 6));
    return list;
}

QString fileStem(const QString& name) {
    QString stem = name;
    stem.replace(QRegularExpression("[^A-Za-z0-9_-]+"), "_");
    return stem.isEmpty() ? QString("theme") : stem;
}

const QStringList& baseKeys() {
    static const QStringList keys = {"canvas", "surface", "raised", "line", "lineStrong", "ink", "inkMuted", "inkSubtle", "accent"};
    return keys;
}

QColor* baseColor(Theme& theme, const QString& key) {
    if (key == "canvas") return &theme.canvas;
    if (key == "surface") return &theme.surface;
    if (key == "raised") return &theme.raised;
    if (key == "line") return &theme.line;
    if (key == "lineStrong") return &theme.lineStrong;
    if (key == "ink") return &theme.ink;
    if (key == "inkMuted") return &theme.inkMuted;
    if (key == "inkSubtle") return &theme.inkSubtle;
    if (key == "accent") return &theme.accent;
    return nullptr;
}

const char* kStyleSheetTemplate = R"(
QWidget { background: @canvas@; color: @inkMuted@; font-family: 'Segoe UI', 'Inter', sans-serif; font-size: @fs@px; }
QMainWindow { background: @canvas@; }
QMainWindow::separator { background: @line@; width: 4px; height: 4px; }
QMainWindow::separator:hover { background: @accent@; }
QDockWidget { border: 1px solid @line@; background: @surface@; color: @inkMuted@; }
QDockWidget::title { padding: 5px 8px; background: @surface@; border-bottom: 1px solid @line@; color: @inkMuted@; font-weight: 600; text-align: left; }
QMenuBar { background: @surface@; border-bottom: 1px solid @line@; padding: 2px 4px; }
QMenuBar::item { padding: 4px 8px; margin: 1px; border-radius: @r@px; }
QMenuBar::item:selected { background: @raised@; color: @accentHover@; }
QMenu { background: @surface@; border: 1px solid @lineStrong@; padding: 4px; }
QMenu::item { padding: 5px 24px 5px 12px; border-radius: @r@px; }
QMenu::item:selected { background: @accent@; color: @accentInk@; }
QMenu::item:disabled { color: @inkDisabled@; }
QMenu::separator { height: 1px; margin: 4px 6px; background: @line@; }
QToolBar { background: @surface@; border: 0; border-bottom: 1px solid @line@; padding: 3px; spacing: 4px; }
QToolButton, QPushButton { padding: 5px 9px; border: 1px solid @lineStrong@; border-radius: @r@px; background: @raised@; color: @ink@; font-weight: 600; }
QToolButton:hover, QPushButton:hover { border-color: @accent@; background: @raisedHover@; }
QToolButton:pressed, QPushButton:pressed { background: @accent@; color: @accentInk@; }
QToolButton:checked { border-color: @accent@; background: @accentSelected@; color: @accentLight@; }
QPushButton:disabled { border-color: @line@; background: @surface@; color: @inkDisabled@; }
QLineEdit, QComboBox, QAbstractSpinBox { padding: 4px 7px; border: 1px solid @lineStrong@; border-radius: @r@px; background: @canvas@; color: @ink@; selection-background-color: @accent@; selection-color: @accentInk@; }
QLineEdit:focus, QComboBox:focus, QAbstractSpinBox:focus { border-color: @accent@; }
QComboBox::drop-down { border: 0; width: 18px; }
QComboBox QAbstractItemView { background: @surface@; border: 1px solid @lineStrong@; selection-background-color: @accent@; selection-color: @accentInk@; }
QTabWidget::pane { border: 1px solid @line@; background: @surface@; }
QTabBar::tab { padding: 6px 12px; margin-right: 1px; border: 1px solid @line@; border-bottom: 0; background: @surface@; color: @inkSubtle@; font-weight: 600; }
QTabBar::tab:selected { border-top: 2px solid @accent@; background: @raised@; color: @ink@; }
QTabBar::tab:hover:!selected { color: @accentHover@; }
QListWidget, QTreeWidget, QTableWidget { padding: 2px; border: 1px solid @line@; border-radius: @r@px; background: @canvas@; }
QListWidget::item, QTreeWidget::item { padding: 4px 6px; }
QListWidget::item:hover, QTreeWidget::item:hover { background: @raised@; }
QListWidget::item:selected, QTreeWidget::item:selected, QTableWidget::item:selected { border: 1px solid @accent@; background: @accentSelected@; color: @ink@; }
QHeaderView::section { padding: 4px; border: 1px solid @line@; background: @raised@; color: @inkMuted@; font-weight: 700; }
QCheckBox { spacing: 6px; }
QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid @lineStrong@; border-radius: @r@px; background: @canvas@; }
QCheckBox::indicator:checked { border-color: @accent@; background: @accent@; }
QSlider::groove:horizontal { height: 3px; background: @line@; }
QSlider::handle:horizontal { width: 12px; margin: -5px 0; border-radius: 6px; background: @accent@; }
QScrollBar:vertical { width: 10px; background: @canvas@; }
QScrollBar:horizontal { height: 10px; background: @canvas@; }
QScrollBar::handle { min-height: 20px; min-width: 20px; border-radius: @r@px; background: @lineStrong@; }
QScrollBar::handle:hover { background: @accent@; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QToolTip { padding: 5px 8px; border: 1px solid @lineStrong@; background: @raised@; color: @ink@; }
QStatusBar { border-top: 1px solid @line@; background: @surface@; color: @inkSubtle@; font-size: 10px; }
QStatusBar::item { border: 0; padding: 2px 6px; }
QWidget#mediaContainer, QWidget#effectsContainer, QWidget#activeContainer, QWidget#tracksContainer, QWidget#controlContainer { border: 1px solid @line@; border-radius: @r@px; background: @surface@; }
)";

} // namespace

ThemePalette Theme::resolve() const {
    if (fixedPalette) return *fixedPalette;
    ThemePalette p;
    p.canvas = canvas; p.surface = surface; p.raised = raised;
    p.line = line; p.lineStrong = lineStrong;
    p.ink = ink; p.inkMuted = inkMuted; p.inkSubtle = inkSubtle;
    p.accent = accent;
    p.raisedHover = mix(raised, ink, 0.07);
    p.inkDisabled = mix(inkSubtle, canvas, 0.45);
    p.accentHover = mix(accent, Qt::white, 0.22);
    p.accentLight = mix(accent, Qt::white, 0.55);
    p.accentSelected = mix(canvas, accent, 0.28);
    p.accentSoft = mix(canvas, accent, 0.16);
    p.accentSoftBorder = mix(canvas, accent, 0.50);
    p.accentSoftHover = mix(canvas, accent, 0.30);
    p.accentInk = luminance(accent) > 0.62 ? QColor("#14141A") : QColor("#FFFFFF");
    p.audio = QColor("#2DBEBF");
    p.audioFill = mix(canvas, p.audio, 0.22);
    return p;
}

QJsonObject Theme::toJson() const {
    QJsonObject colors;
    Theme copy = *this;
    for (const QString& key : baseKeys()) colors.insert(key, baseColor(copy, key)->name(QColor::HexRgb));
    QJsonObject object;
    object.insert("name", name);
    object.insert("cornerRadius", cornerRadius);
    object.insert("fontSize", fontSize);
    object.insert("colors", colors);
    return object;
}

std::optional<Theme> Theme::fromJson(const QJsonObject& object) {
    const QString themeName = object.value("name").toString().trimmed();
    const QJsonObject colors = object.value("colors").toObject();
    if (themeName.isEmpty() || colors.isEmpty()) return std::nullopt;

    Theme theme;
    theme.name = themeName;
    for (const QString& key : baseKeys()) {
        const QColor color(colors.value(key).toString());
        if (!color.isValid()) return std::nullopt;
        *baseColor(theme, key) = color;
    }
    theme.cornerRadius = qBound(0, object.value("cornerRadius").toInt(3), 12);
    theme.fontSize = qBound(9, object.value("fontSize").toInt(11), 16);
    return theme;
}

ThemedColor::ThemedColor(const char* hex) : QColor(ThemeManager::instance().remapColor(QColor(hex))) {}
ThemedColor::ThemedColor(const QString& hex) : QColor(ThemeManager::instance().remapColor(QColor(hex))) {}
ThemedColor::ThemedColor(int r, int g, int b, int a) : QColor(ThemeManager::instance().remapColor(QColor(r, g, b, a))) {}
ThemedColor::ThemedColor(const QColor& color) : QColor(ThemeManager::instance().remapColor(color)) {}

ThemeManager& ThemeManager::instance() {
    static ThemeManager manager;
    return manager;
}

QString ThemeManager::themesDirectory() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/themes";
}

void ThemeManager::initialize(QApplication& app) {
    application = &app;
    app.setStyle(QStyleFactory::create("Fusion"));
    themes = builtInThemes();
    loadCustomThemes();
    app.installEventFilter(this);

    const QString saved = QSettings("Z-Creative", "Z").value("Appearance/Theme", kDefaultThemeName).toString();
    const Theme* theme = find(saved);
    currentTheme = theme ? *theme : themes.front();
    currentPalette = currentTheme.resolve();
    applyCurrent();
}

void ThemeManager::loadCustomThemes() {
    const QDir dir(themesDirectory());
    for (const QFileInfo& info : dir.entryInfoList({"*.json"}, QDir::Files, QDir::Name)) {
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) continue;
        const auto document = QJsonDocument::fromJson(file.readAll());
        auto theme = Theme::fromJson(document.object());
        if (!theme || find(theme->name)) continue;
        themes.push_back(*theme);
    }
}

QStringList ThemeManager::themeNames() const {
    QStringList names;
    for (const Theme& theme : themes) names << theme.name;
    return names;
}

const Theme* ThemeManager::find(const QString& name) const {
    for (const Theme& theme : themes) {
        if (theme.name == name) return &theme;
    }
    return nullptr;
}

bool ThemeManager::setCurrent(const QString& name, bool persist) {
    const Theme* theme = find(name);
    if (!theme) return false;
    currentTheme = *theme;
    currentPalette = currentTheme.resolve();
    if (persist) QSettings("Z-Creative", "Z").setValue("Appearance/Theme", currentTheme.name);
    applyCurrent();
    return true;
}

void ThemeManager::preview(const Theme& theme) {
    currentTheme = theme;
    currentPalette = currentTheme.resolve();
    applyCurrent();
}

QString ThemeManager::uniqueName(const QString& base) const {
    auto taken = [this](const QString& candidate) {
        for (const Theme& theme : themes) {
            if (theme.name.compare(candidate, Qt::CaseInsensitive) == 0 || fileStem(theme.name) == fileStem(candidate)) return true;
        }
        return false;
    };
    QString candidate = base;
    for (int i = 2; taken(candidate); ++i) candidate = QString("%1 %2").arg(base).arg(i);
    return candidate;
}

bool ThemeManager::saveCustomTheme(const Theme& theme, QString* error) {
    auto fail = [error](const QString& message) {
        if (error) *error = message;
        return false;
    };
    const QString name = theme.name.trimmed();
    if (name.isEmpty()) return fail("Theme name cannot be empty.");

    auto existing = std::find_if(themes.begin(), themes.end(), [&](const Theme& t) { return t.name == name; });
    if (existing != themes.end() && existing->builtIn) return fail("Built-in themes cannot be overwritten.");
    for (const Theme& other : themes) {
        if (other.name != name && fileStem(other.name) == fileStem(name)) return fail("Another theme uses a conflicting name.");
    }

    QDir().mkpath(themesDirectory());
    Theme stored = theme;
    stored.name = name;
    stored.builtIn = false;
    stored.fixedPalette.reset();

    QSaveFile file(themesDirectory() + "/" + fileStem(name) + ".json");
    if (!file.open(QIODevice::WriteOnly)) return fail("Could not write the theme file.");
    file.write(QJsonDocument(stored.toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) return fail("Could not write the theme file.");

    if (existing != themes.end()) *existing = stored;
    else themes.push_back(stored);
    return true;
}

bool ThemeManager::deleteCustomTheme(const QString& name) {
    auto it = std::find_if(themes.begin(), themes.end(), [&](const Theme& t) { return t.name == name && !t.builtIn; });
    if (it == themes.end()) return false;
    QFile::remove(themesDirectory() + "/" + fileStem(name) + ".json");
    const bool wasCurrent = currentTheme.name == name;
    themes.erase(it);
    if (wasCurrent) setCurrent(kDefaultThemeName);
    return true;
}

bool ThemeManager::importTheme(const QString& path, Theme* out, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = "Could not open the theme file.";
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    auto theme = parseError.error == QJsonParseError::NoError ? Theme::fromJson(document.object()) : std::nullopt;
    if (!theme) {
        if (error) *error = "This is not a valid Z theme file.";
        return false;
    }
    theme->name = uniqueName(theme->name);
    if (!saveCustomTheme(*theme, error)) return false;
    if (out) *out = *theme;
    return true;
}

bool ThemeManager::exportTheme(const Theme& theme, const QString& path, QString* error) const {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = "Could not write the theme file.";
        return false;
    }
    Theme exported = theme;
    if (exported.fixedPalette) {
        // Built-in palettes may not be derivable from the base colours, so export the base colours as-is.
        exported.fixedPalette.reset();
    }
    file.write(QJsonDocument(exported.toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) *error = "Could not write the theme file.";
        return false;
    }
    return true;
}

QColor ThemeManager::remapColor(const QColor& color) const {
    if (!color.isValid() || currentTheme.name.isEmpty()) return color;
    const auto it = defaultTokens().constFind(color.rgb());
    if (it == defaultTokens().constEnd()) return color;
    QColor mapped = currentPalette.*(it.value());
    mapped.setAlpha(color.alpha());
    return mapped;
}

QString ThemeManager::remapStyleSheet(const QString& styleSheet) const {
    static const QRegularExpression hexColor("#[0-9A-Fa-f]{6}(?![0-9A-Fa-f])");
    QString result;
    result.reserve(styleSheet.size());
    qsizetype last = 0;
    auto matches = hexColor.globalMatch(styleSheet);
    while (matches.hasNext()) {
        const auto match = matches.next();
        result += styleSheet.mid(last, match.capturedStart() - last);
        result += remapColor(QColor(match.captured())).name(QColor::HexRgb).toUpper();
        last = match.capturedEnd();
    }
    result += styleSheet.mid(last);
    return result;
}

QString ThemeManager::buildStyleSheet() const {
    const ThemePalette& p = currentPalette;
    QString sheet = kStyleSheetTemplate;
    const std::vector<std::pair<QString, QString>> tokens = {
        {"@canvas@", p.canvas.name()}, {"@surface@", p.surface.name()}, {"@raisedHover@", p.raisedHover.name()},
        {"@raised@", p.raised.name()}, {"@lineStrong@", p.lineStrong.name()}, {"@line@", p.line.name()},
        {"@inkMuted@", p.inkMuted.name()}, {"@inkSubtle@", p.inkSubtle.name()}, {"@inkDisabled@", p.inkDisabled.name()},
        {"@ink@", p.ink.name()}, {"@accentHover@", p.accentHover.name()}, {"@accentLight@", p.accentLight.name()},
        {"@accentSelected@", p.accentSelected.name()}, {"@accentInk@", p.accentInk.name()}, {"@accent@", p.accent.name()},
        {"@r@", QString::number(currentTheme.cornerRadius)}, {"@fs@", QString::number(currentTheme.fontSize)},
    };
    for (const auto& [token, value] : tokens) sheet.replace(token, value);
    return sheet;
}

void ThemeManager::applyCurrent() {
    if (!application) return;
    const ThemePalette& p = currentPalette;

    QPalette palette;
    palette.setColor(QPalette::Window, p.canvas);
    palette.setColor(QPalette::WindowText, p.ink);
    palette.setColor(QPalette::Base, p.canvas);
    palette.setColor(QPalette::AlternateBase, p.raised);
    palette.setColor(QPalette::ToolTipBase, p.raised);
    palette.setColor(QPalette::ToolTipText, p.ink);
    palette.setColor(QPalette::Text, p.inkMuted);
    palette.setColor(QPalette::Button, p.raised);
    palette.setColor(QPalette::ButtonText, p.ink);
    palette.setColor(QPalette::BrightText, p.accentHover);
    palette.setColor(QPalette::Link, p.accent);
    palette.setColor(QPalette::Highlight, p.accent);
    palette.setColor(QPalette::HighlightedText, p.accentInk);
    palette.setColor(QPalette::Disabled, QPalette::Text, p.inkDisabled);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, p.inkDisabled);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, p.inkDisabled);
    application->setPalette(palette);
    application->setStyleSheet(buildStyleSheet());

    for (QWidget* widget : application->allWidgets()) {
        const QString base = widget->property(kBaseProperty).toString();
        if (base.isEmpty()) {
            adoptWidget(widget);
        } else {
            const QString remapped = remapStyleSheet(base);
            widget->setProperty(kAppliedProperty, remapped);
            if (widget->styleSheet() != remapped) widget->setStyleSheet(remapped);
        }
        widget->update();
    }
    emit themeChanged();
}

// Widgets set their own inline style sheets using the default colours; remember the
// original text so it can be re-mapped whenever the theme changes.
void ThemeManager::adoptWidget(QWidget* widget) {
    const QString sheet = widget->styleSheet();
    if (sheet.isEmpty() || widget->property(kAppliedProperty).toString() == sheet) return;
    const QString remapped = remapStyleSheet(sheet);
    widget->setProperty(kBaseProperty, sheet);
    widget->setProperty(kAppliedProperty, remapped);
    if (remapped != sheet) widget->setStyleSheet(remapped);
}

bool ThemeManager::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::StyleChange && watched->isWidgetType()) {
        adoptWidget(static_cast<QWidget*>(watched));
    }
    return QObject::eventFilter(watched, event);
}
