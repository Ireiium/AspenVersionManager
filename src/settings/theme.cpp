#include "theme.h"
#include "assoc/registry.h"

#include <QApplication>
#include <QMap>
#include <QPalette>
#include <QSettings>
#include <QStringList>
#include <QStyleHints>
#include <QTimer>
#include <algorithm>
#include <windows.h>

themeColors theme::colors;

theme::theme(QObject* parent) : QObject(parent)
{
    QSettings settings;
    const int saved = settings.value("themeMode", 0).toInt();
    selectedMode = static_cast<themeMode>(qBound(0, saved, 2));
    qApp->installNativeEventFilter(this);
    apply();
}

theme::~theme()
{
    qApp->removeNativeEventFilter(this);
}

themeMode theme::mode() const
{
    return selectedMode;
}

const themeColors& theme::tokens()
{
    return colors;
}

void theme::setMode(themeMode value)
{
    selectedMode = value;
    QSettings settings;
    settings.setValue("themeMode", static_cast<int>(value));
    settings.sync();
    apply();
}

bool theme::nativeEventFilter(const QByteArray&, void* message, qintptr*)
{
    const MSG* event = static_cast<MSG*>(message);
    if ((event->message == WM_SETTINGCHANGE || event->message == WM_THEMECHANGED ||
         event->message == WM_DWMCOLORIZATIONCOLORCHANGED) &&
        !queued)
    {
        queued = true;
        QTimer::singleShot(0, this,
                           [this]
                           {
                               queued = false;
                               apply();
                           });
    }
    return false;
}

void theme::apply()
{
    registry reg;
    const regValue pref = reg.read(
        "HKCU", "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", "AppsUseLightTheme");
    DWORD light = 1;
    if (pref.exists && pref.type == REG_DWORD && pref.data.size() == 4)
    {
        memcpy(&light, pref.data.constData(), 4);
    }
    const bool dark = selectedMode == themeMode::dark || (selectedMode == themeMode::system && !light);
    qApp->styleHints()->setColorScheme(selectedMode == themeMode::system
                                           ? Qt::ColorScheme::Unknown
                                           : (dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light));
    colors.bg = QColor(dark ? "#191C22" : "#F5F7FA");
    colors.surface = QColor(dark ? "#242932" : "#FFFFFF");
    colors.text = QColor(dark ? "#E8EDF5" : "#202630");
    colors.muted = QColor(dark ? "#AEB8C8" : "#5E6B7D");
    colors.border = QColor(dark ? "#404958" : "#DCE3EC");
    colors.hover = QColor(dark ? "#303844" : "#EEF3F9");
    colors.pressed = QColor(dark ? "#344357" : "#DFEAF8");
    colors.disabled = QColor(dark ? "#7D899B" : "#8793A3");
    // Product blue is independent of the Windows accent; only the light/dark mode follows the system.
    colors.accent = QColor("#1769AA");
    colors.accentHover = QColor("#125C98");
    colors.accentPress = QColor("#0D4F85");
    colors.focus = QColor(dark ? "#83BCF0" : "#1769AA");
    colors.selected = QColor(dark ? "#20374F" : "#EAF2FC");
    colors.selectedText = QColor(dark ? "#C6E2FF" : "#174875");
    colors.selectedBorder = QColor(dark ? "#527AA3" : "#A4C3E6");
    colors.success = QColor(dark ? "#B2E8CC" : "#15553F");
    colors.successBg = QColor(dark ? "#193A2C" : "#EAF7F0");
    colors.warning = QColor(dark ? "#F0C789" : "#82510A");
    colors.warningBg = QColor(dark ? "#3C3020" : "#FFF3DC");
    colors.error = QColor(dark ? "#F3B4AD" : "#9A3029");
    colors.errorBg = QColor(dark ? "#442925" : "#FCEBE8");
    colors.shadow = QColor(0, 0, 0, dark ? 90 : 35);

    QPalette palette;
    palette.setColor(QPalette::Window, colors.bg);
    palette.setColor(QPalette::WindowText, colors.text);
    palette.setColor(QPalette::Base, colors.surface);
    palette.setColor(QPalette::AlternateBase, colors.hover);
    palette.setColor(QPalette::Text, colors.text);
    palette.setColor(QPalette::Button, colors.surface);
    palette.setColor(QPalette::ButtonText, colors.text);
    palette.setColor(QPalette::Highlight, colors.selected);
    palette.setColor(QPalette::HighlightedText, colors.selectedText);
    palette.setColor(QPalette::Link, colors.focus);
    palette.setColor(QPalette::ToolTipBase, colors.surface);
    palette.setColor(QPalette::ToolTipText, colors.text);
    palette.setColor(QPalette::PlaceholderText, colors.muted);
    palette.setColor(QPalette::Disabled, QPalette::Text, colors.disabled);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, colors.disabled);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, colors.disabled);
    qApp->setPalette(palette);

    QString css = R"(
        QWidget { color: @text; font-size: @bodyFontpt; }
        QMainWindow, QDialog { background: @bg; }
        QWidget#sideBar { background: @surface; border-right: 1px solid @border; }
        QFrame[role="card"] { background: @surface; border: 1px solid @border; border-radius: @panelRadiuspx; }
        QFrame[role="card"][selected="true"] { background: @selected; border-color: @selectedBorder; }
        QFrame[role="settingRow"] { background: transparent; border: none; border-bottom: 1px solid @border; }
        QLabel { background: transparent; border: none; }
        QLabel[role="title"] { font-size: @titleFontpt; font-weight: 600; }
        QLabel[role="heading"] { font-size: @headingFontpt; font-weight: 600; }
        QLabel[role="muted"] { color: @muted; font-size: @smallFontpt; }
        QLabel[role="badge"] { padding: @space4px @space8px; border-radius: @controlRadiuspx; background: @hover; color: @muted; font-size: @smallFontpt; }
        QLabel[role="badge"][tone="good"] { background: @successBg; color: @success; }
        QLabel[role="badge"][tone="selected"] { background: @selected; color: @selectedText; }
        QLabel[role="warning"] { color: @warning; font-size: @smallFontpt; }
        QLabel[role="badge"][tone="warning"] { background: @warningBg; color: @warning; }
        QLabel[role="badge"][tone="error"] { background: @errorBg; color: @error; }
        QPushButton { background: @surface; border: 1px solid @border; border-radius: @controlRadiuspx; padding: @space4px @space12px; }
        QPushButton:hover { background: @hover; }
        QPushButton:pressed { background: @pressed; }
        QPushButton:focus, QComboBox:focus, QLineEdit:focus, QListWidget:focus, QPlainTextEdit:focus, QTextEdit:focus { border-color: @focus; }
        QPushButton[role="quiet"] { background: transparent; border-color: transparent; }
        QPushButton[role="quiet"]:hover { background: @hover; }
        QPushButton[role="quiet"]:pressed { background: @pressed; }
        QPushButton[role="quiet"]:focus { border-color: @focus; }
        QPushButton[role="link"] { background: transparent; border-color: transparent; color: @focus; text-align: left; padding: @space4px; }
        QPushButton[role="link"]:hover { background: @hover; }
        QPushButton[role="link"]:pressed { background: @pressed; }
        QPushButton[role="link"]:focus { border-color: @focus; }
        QPushButton[role="primary"] { background: @accent; border-color: @accent; color: white; font-weight: 600; }
        QPushButton[role="primary"]:hover { background: @accentHover; border-color: @accentHover; }
        QPushButton[role="primary"]:pressed { background: @accentPress; border-color: @accentPress; }
        QPushButton:disabled, QPushButton[role="primary"]:disabled { background: @hover; color: @disabled; border-color: @border; }
        QPushButton[role="nav"] { border-color: transparent; text-align: left; padding: @space8px @space12px; background: transparent; }
        QPushButton[role="nav"]:hover { background: @hover; }
        QPushButton[role="nav"]:checked { background: @selected; color: @selectedText; font-weight: 600; }
        QPushButton[role="nav"]:focus { border-color: @focus; }
        QComboBox, QLineEdit, QListWidget, QPlainTextEdit, QTextEdit, QTreeView { background: @surface; border: 1px solid @border; border-radius: @controlRadiuspx; padding: @space4px; selection-background-color: @selected; selection-color: @selectedText; }
        QLineEdit[role="path"] { background: transparent; border: 1px solid transparent; color: @muted; font-size: @smallFontpt; padding: 0; }
        QLineEdit { min-height: 26px; }
        QLineEdit[role="path"] { min-height: 0; }
        QLineEdit[role="path"]:focus { border-color: @focus; }
        QComboBox { min-height: 26px; padding-left: @space8px; padding-right: @space24px; }
        QComboBox QAbstractItemView { background: @surface; color: @text; selection-background-color: @selected; selection-color: @selectedText; }
        QListWidget::item { padding: @space8px; border-radius: @controlRadiuspx; }
        QListWidget::item:hover { background: @hover; }
        QListWidget::item:selected { background: @selected; color: @selectedText; }
        QListView#comboList { background: @surface; border: 1px solid @border; border-radius: @controlRadiuspx; padding: @space4px; outline: none; }
        QFrame#comboPopup { background: transparent; border: none; }
        QScrollArea { border: none; background: transparent; }
        QScrollArea > QWidget > QWidget { background: transparent; }
        QScrollBar:vertical { background: transparent; width: @space8px; margin: 2px; }
        QScrollBar::handle:vertical { background: @border; border-radius: 3px; min-height: @space24px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QScrollBar:horizontal { background: transparent; height: @space8px; margin: 2px; }
        QScrollBar::handle:horizontal { background: @border; border-radius: 3px; min-width: @space24px; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }
        QTreeView::item:hover { background: @hover; }
        QTreeView::item:selected { background: @selected; color: @selectedText; }
        QToolTip, QMenu { background: @surface; color: @text; border: 1px solid @border; padding: @space4px; }
        QMenu::item:selected { background: @selected; color: @selectedText; }
        QHeaderView::section { background: @hover; color: @muted; border: none; padding: @space8px; }
    )";
    QMap<QString, QString> values{{"@bg", colors.bg.name()},
                                  {"@surface", colors.surface.name()},
                                  {"@text", colors.text.name()},
                                  {"@muted", colors.muted.name()},
                                  {"@border", colors.border.name()},
                                  {"@hover", colors.hover.name()},
                                  {"@pressed", colors.pressed.name()},
                                  {"@disabled", colors.disabled.name()},
                                  {"@accent", colors.accent.name()},
                                  {"@accentHover", colors.accentHover.name()},
                                  {"@accentPress", colors.accentPress.name()},
                                  {"@focus", colors.focus.name()},
                                  {"@selected", colors.selected.name()},
                                  {"@selectedText", colors.selectedText.name()},
                                  {"@selectedBorder", colors.selectedBorder.name()},
                                  {"@success", colors.success.name()},
                                  {"@successBg", colors.successBg.name()},
                                  {"@warning", colors.warning.name()},
                                  {"@warningBg", colors.warningBg.name()},
                                  {"@error", colors.error.name()},
                                  {"@errorBg", colors.errorBg.name()}};
    values["@bodyFont"] = QString::number(uiSize::bodyFont);
    values["@smallFont"] = QString::number(uiSize::smallFont);
    values["@headingFont"] = QString::number(uiSize::headingFont);
    values["@titleFont"] = QString::number(uiSize::titleFont);
    values["@space4"] = QString::number(uiSize::space4);
    values["@space8"] = QString::number(uiSize::space8);
    values["@space12"] = QString::number(uiSize::space12);
    values["@space24"] = QString::number(uiSize::space24);
    values["@controlRadius"] = QString::number(uiSize::controlRadius);
    values["@panelRadius"] = QString::number(uiSize::panelRadius);
    QStringList names = values.keys();
    std::sort(names.begin(), names.end(),
              [](const QString& a, const QString& b) { return a.size() > b.size(); });
    for (const QString& name : names)
    {
        css.replace(name, values[name]);
    }
    qApp->setStyleSheet(css);
    emit changed();
}
