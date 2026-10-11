#pragma once

#include <QAbstractNativeEventFilter>
#include <QColor>
#include <QObject>

namespace uiSize
{
inline constexpr int space4 = 4;
inline constexpr int space8 = 8;
inline constexpr int space12 = 12;
inline constexpr int space16 = 16;
inline constexpr int space24 = 24;
inline constexpr int controlHeight = 36;
inline constexpr int buttonWidth = 96;
inline constexpr int optionHeight = 36;
inline constexpr int controlRadius = 6;
inline constexpr int panelRadius = 8;
inline constexpr int popupGap = 5;
inline constexpr int shadowPad = 8;
inline constexpr int sideWidth = 184;
inline constexpr int selectWidth = 180;
inline constexpr int bodyFont = 10;
inline constexpr int smallFont = 9;
inline constexpr int headingFont = 12;
inline constexpr int titleFont = 16;
} // namespace uiSize

struct themeColors
{
    QColor bg, surface, text, muted, border, hover, pressed, disabled;
    QColor accent, accentHover, accentPress, focus;
    QColor selected, selectedText, selectedBorder;
    QColor success, successBg;
    QColor warning, warningBg, error, errorBg, shadow;
};

enum class themeMode
{
    system,
    light,
    dark
};

class theme : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

    public:
    explicit theme(QObject* parent = nullptr);
    ~theme() override;
    themeMode mode() const;
    static const themeColors& tokens();
    void setMode(themeMode value);
    void apply();
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

    signals:
    void changed();

    private:
    themeMode selectedMode = themeMode::system;
    bool queued = false;
    static themeColors colors;
};
