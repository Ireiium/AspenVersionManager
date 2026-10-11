#pragma once

#include <QObject>
#include <QTranslator>

enum class langMode
{
    chinese = 1,
    english = 2
};

class language : public QObject
{
    Q_OBJECT

    public:
    static language& instance();
    ~language() override;
    void setMode(langMode mode);

    signals:
    void changed();

    protected:
    bool eventFilter(QObject* object, QEvent* event) override;

    private:
    explicit language(QObject* parent);
    langMode selected = langMode::chinese;
    bool english = false;
    bool ready = false;
    QTranslator appTexts;
    QTranslator qtTexts;
    static langMode systemMode(const QString& name);
    void apply();
};

// Widgets retain source text so LanguageChange updates existing objects and input state.
void bindText(QObject* object, const QString& source, const char* property = "text");
