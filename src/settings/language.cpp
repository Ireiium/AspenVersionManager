#include "language.h"

#include <QCoreApplication>
#include <QEvent>
#include <QLibraryInfo>
#include <QLocale>
#include <QPointer>
#include <QSettings>
#include <QStringList>
#include <QVariantMap>

namespace
{
void updateText(QObject* object)
{
    const QVariantMap texts = object->property("uiTexts").toMap();
    for (auto item = texts.cbegin(); item != texts.cend(); ++item)
    {
        const QByteArray source = item.value().toString().toUtf8();
        object->setProperty(item.key().toUtf8().constData(), QObject::tr(source.constData()));
    }
}
} // namespace

void bindText(QObject* object, const QString& source, const char* property)
{
    QVariantMap texts = object->property("uiTexts").toMap();
    texts[QString::fromLatin1(property)] = source;
    object->setProperty("uiTexts", texts);
    updateText(object);
}

language& language::instance()
{
    static QPointer<language> current;
    if (!current)
    {
        current = new language(QCoreApplication::instance());
    }
    return *current;
}

language::language(QObject* parent) : QObject(parent)
{
    Q_INIT_RESOURCE(lang);
    QSettings settings;
    const int saved = settings.value("languageMode", 0).toInt();
    if (saved == static_cast<int>(langMode::chinese) || saved == static_cast<int>(langMode::english))
    {
        selected = static_cast<langMode>(saved);
    }
    else
    {
        // Resolve once, including the previous system-following setting (0).
        const QLocale system = QLocale::system();
        const QStringList names = system.uiLanguages();
        selected = systemMode(names.isEmpty() ? system.name() : names.first());
        settings.setValue("languageMode", static_cast<int>(selected));
        settings.sync();
    }
    QCoreApplication::instance()->installEventFilter(this);
    apply();
}

language::~language()
{
    if (QCoreApplication* app = QCoreApplication::instance())
    {
        app->removeEventFilter(this);
        QCoreApplication::removeTranslator(&appTexts);
        QCoreApplication::removeTranslator(&qtTexts);
    }
}

langMode language::systemMode(const QString& name)
{
    return QLocale(name).language() == QLocale::English ? langMode::english : langMode::chinese;
}

langMode language::mode() const
{
    return selected;
}

void language::setMode(langMode mode)
{
    selected = mode == langMode::english ? langMode::english : langMode::chinese;
    QSettings settings;
    settings.setValue("languageMode", static_cast<int>(selected));
    settings.sync();
    apply();
    emit changed();
}

void language::apply()
{
    const bool next = selected == langMode::english;
    if (ready && next == english)
    {
        return;
    }

    QCoreApplication::removeTranslator(&appTexts);
    QCoreApplication::removeTranslator(&qtTexts);
    english = next;
    ready = true;
    if (english)
    {
        if (appTexts.load(":/i18n/app_en.qm"))
        {
            QCoreApplication::installTranslator(&appTexts);
        }
    }
    else
    {
        const QString folder = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
        if (qtTexts.load("qtbase_zh_CN", folder) || qtTexts.load("qt_zh_CN", folder))
        {
            QCoreApplication::installTranslator(&qtTexts);
        }
    }
}

bool language::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        updateText(object);
    }
    return false;
}
