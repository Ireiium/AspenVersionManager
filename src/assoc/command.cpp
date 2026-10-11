#include <QObject>
#include "command.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <windows.h>

QString expandEnv(const QString& text)
{
    const auto input = text.toStdWString();
    const DWORD size = ExpandEnvironmentStringsW(input.c_str(), nullptr, 0);
    if (!size || size > 32768)
        return text;
    std::wstring output(size, L'\0');
    if (!ExpandEnvironmentStringsW(input.c_str(), output.data(), size))
        return text;
    return QString::fromWCharArray(output.c_str());
}

QString cleanPath(const QString& path)
{
    if (path.trimmed().isEmpty())
        return {};
    return QDir::cleanPath(QDir::fromNativeSeparators(expandEnv(path.trimmed())));
}

bool samePath(const QString& left, const QString& right)
{
    if (left.isEmpty() || right.isEmpty())
        return false;
    const QFileInfo a(cleanPath(left)), b(cleanPath(right));
    const QString first = a.exists() ? a.canonicalFilePath() : a.absoluteFilePath();
    const QString second = b.exists() ? b.canonicalFilePath() : b.absoluteFilePath();
    return first.compare(second, Qt::CaseInsensitive) == 0;
}

openCommand parseCommand(const QString& text)
{
    // Legacy Aspen registrations use unquoted paths. Callers verify the resulting
    // executable against Shell resolution and product metadata before writing a quoted path.
    static const QRegularExpression pattern(R"rx(^\s*(?:"([^"]+\.exe)"|([^"\r\n]+?\.exe))(\s+.*)?\s*$)rx",
                                            QRegularExpression::CaseInsensitiveOption);
    const auto match = pattern.match(text);
    if (!match.hasMatch())
        return {};
    return {cleanPath(match.captured(1).isEmpty() ? match.captured(2) : match.captured(1)), match.captured(3),
            true};
}

bool sameCommand(const QString& left, const QString& right)
{
    const openCommand first = parseCommand(expandEnv(left));
    const openCommand second = parseCommand(expandEnv(right));

    // Shell may format the executable differently. Arguments must still match exactly.
    return first.valid && second.valid && samePath(first.exePath, second.exePath) &&
           first.args == second.args;
}

QString replaceExe(const QString& text, const QString& exePath, QString& error)
{
    const auto parsed = parseCommand(text);
    static const QRegularExpression fileArg(R"rx("%(?:1|l)")rx", QRegularExpression::CaseInsensitiveOption);
    if (!parsed.valid || !fileArg.match(parsed.args).hasMatch())
    {
        error = QObject::tr("打开命令不明确或文件参数没有引号，无法安全保留参数。");
        return {};
    }
    if (!QFileInfo(exePath).isAbsolute() || exePath.contains('"'))
    {
        error = QObject::tr("程序路径必须是没有引号的绝对路径。");
        return {};
    }
    return '"' + QDir::toNativeSeparators(cleanPath(exePath)) + '"' + parsed.args;
}
