#include <QObject>
#include "registry.h"
#include "command.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>

namespace
{
HKEY rootKey(const QString& root)
{
    if (root == "HKCU")
        return HKEY_CURRENT_USER;
    if (root == "HKLM")
        return HKEY_LOCAL_MACHINE;
    return nullptr;
}

REGSAM viewFlag(int view)
{
    return view == 64 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY;
}

QString winError(LSTATUS code)
{
    wchar_t* buffer = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    const QString detail = buffer ? QString::fromWCharArray(buffer).trimmed() : QString();
    LocalFree(buffer);
    return QStringLiteral("Win32 %1: %2%3")
        .arg(code)
        .arg(detail, code == ERROR_ACCESS_DENIED
                         ? QObject::tr("。需要管理员权限，请右键工具选择“以管理员身份运行”。")
                         : QString());
}
} // namespace

QString registry::shellText(const QString& fileExt, int field, bool open) const
{
    DWORD size = 0;
    const auto ext = fileExt.toStdWString();
    AssocQueryStringW(ASSOCF_NOTRUNCATE, static_cast<ASSOCSTR>(field), ext.c_str(), open ? L"open" : nullptr,
                      nullptr, &size);
    if (!size || size > 32768)
        return {};
    std::wstring result(size, L'\0');
    if (AssocQueryStringW(ASSOCF_NOTRUNCATE, static_cast<ASSOCSTR>(field), ext.c_str(),
                          open ? L"open" : nullptr, result.data(), &size) != S_OK)
        return {};
    return QString::fromWCharArray(result.c_str());
}

QString registry::classText(const QString& progId, int field, bool open) const
{
    HKEY key = nullptr;
    if (progId.isEmpty() || progId.contains('\\') || progId.contains('/') ||
        RegOpenKeyExW(HKEY_CLASSES_ROOT, progId.toStdWString().c_str(), 0, KEY_READ | KEY_WOW64_64KEY,
                      &key) != ERROR_SUCCESS)
    {
        return {};
    }

    // Use a freshly opened effective class, rather than repeating extension resolution after a write.
    const ASSOCF flags = static_cast<ASSOCF>(ASSOCF_NOTRUNCATE | ASSOCF_NOFIXUPS);
    const ASSOCSTR kind = static_cast<ASSOCSTR>(field);
    const wchar_t* verb = open ? L"open" : nullptr;
    DWORD size = 0;
    const HRESULT sized = AssocQueryStringByKeyW(flags, kind, key, verb, nullptr, &size);
    QString text;
    if ((sized == S_FALSE || sized == E_POINTER) && size > 0 && size <= 32768)
    {
        std::wstring buffer(size, L'\0');
        if (AssocQueryStringByKeyW(flags, kind, key, verb, buffer.data(), &size) == S_OK)
        {
            text = QString::fromWCharArray(buffer.c_str());
        }
    }
    RegCloseKey(key);
    return text;
}

QString regValue::text() const
{
    if (!exists || (type != REG_SZ && type != REG_EXPAND_SZ) || data.size() % 2)
        return {};
    QString output = QString::fromUtf16(reinterpret_cast<const char16_t*>(data.constData()), data.size() / 2);
    while (output.endsWith(QChar(0)))
        output.chop(1);
    return output;
}

QString regValue::id() const
{
    return root + ':' + QString::number(view) + ':' + path.toLower() + ':' + name.toLower();
}

regValue stringValue(regValue value, const QString& text)
{
    value.exists = true;
    value.keyExists = true;
    if (value.type != REG_EXPAND_SZ)
        value.type = REG_SZ;
    value.data = QByteArray(reinterpret_cast<const char*>(text.utf16()), (text.size() + 1) * 2);
    return value;
}

bool sameValue(const regValue& a, const regValue& b)
{
    return a.error.isEmpty() && b.error.isEmpty() && a.id() == b.id() && a.keyExists == b.keyExists &&
           a.exists == b.exists && (!a.exists || (a.type == b.type && a.data == b.data));
}

regValue registry::read(const QString& root, const QString& path, const QString& name, int view) const
{
    regValue result;
    result.root = root;
    result.path = path;
    result.name = name;
    result.view = view;
    HKEY key = nullptr;
    if (!rootKey(root))
    {
        result.error = QObject::tr("无效注册表根路径");
        return result;
    }
    auto code = RegOpenKeyExW(rootKey(root), path.toStdWString().c_str(), 0, KEY_READ | viewFlag(view), &key);
    if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND)
        return result;
    if (code != ERROR_SUCCESS)
    {
        result.error = winError(code);
        return result;
    }
    result.keyExists = true;
    DWORD size = 0, type = 0;
    const auto valueName = name.toStdWString();
    code = RegQueryValueExW(key, valueName.c_str(), nullptr, &type, nullptr, &size);
    if (code == ERROR_SUCCESS && size <= 1024 * 1024)
    {
        result.data.resize(size);
        code = RegQueryValueExW(key, valueName.c_str(), nullptr, &type,
                                reinterpret_cast<BYTE*>(result.data.data()), &size);
        if (code == ERROR_SUCCESS)
        {
            result.exists = true;
            result.type = type;
            result.data.resize(size);
        }
    }
    else if (size > 1024 * 1024)
        code = ERROR_MORE_DATA;
    RegCloseKey(key);
    if (code != ERROR_SUCCESS && code != ERROR_FILE_NOT_FOUND)
        result.error = winError(code);
    return result;
}

QString registry::write(const regValue& value)
{
    // Only existing default command values are writable. No UserChoice, COM or new keys.
    static const QRegularExpression allowed(R"(^Software\\Classes\\[^\\]+\\shell\\open\\command$)",
                                            QRegularExpression::CaseInsensitiveOption);
    if (!rootKey(value.root) || !allowed.match(value.path).hasMatch() || !value.name.isEmpty() ||
        !value.keyExists || !value.exists || (value.view != 64 && value.view != 32) ||
        (value.type != REG_SZ && value.type != REG_EXPAND_SZ))
        return QObject::tr("拒绝写入非现有 Aspen 打开命令项。");
    HKEY key = nullptr;
    auto code = RegOpenKeyExW(rootKey(value.root), value.path.toStdWString().c_str(), 0,
                              KEY_SET_VALUE | viewFlag(value.view), &key);
    if (code != ERROR_SUCCESS)
        return winError(code);
    code = RegSetValueExW(key, nullptr, 0, value.type, reinterpret_cast<const BYTE*>(value.data.constData()),
                          value.data.size());
    if (code == ERROR_SUCCESS)
        code = RegFlushKey(key);
    RegCloseKey(key);
    return code == ERROR_SUCCESS ? QString() : winError(code);
}

QStringList registry::keys(const QString& root, const QString& path, int view) const
{
    QStringList output;
    HKEY key = nullptr;
    if (!rootKey(root) || RegOpenKeyExW(rootKey(root), path.toStdWString().c_str(), 0,
                                        KEY_READ | viewFlag(view), &key) != ERROR_SUCCESS)
        return output;
    for (DWORD index = 0;; ++index)
    {
        wchar_t buffer[512];
        DWORD size = 512;
        const auto code = RegEnumKeyExW(key, index, buffer, &size, nullptr, nullptr, nullptr, nullptr);
        if (code == ERROR_NO_MORE_ITEMS)
            break;
        if (code == ERROR_SUCCESS)
            output.append(QString::fromWCharArray(buffer, size));
        else
            break;
    }
    RegCloseKey(key);
    return output;
}

QStringList registry::names(const QString& root, const QString& path, int view) const
{
    QStringList output;
    HKEY key = nullptr;
    if (!rootKey(root) || RegOpenKeyExW(rootKey(root), path.toStdWString().c_str(), 0,
                                        KEY_READ | viewFlag(view), &key) != ERROR_SUCCESS)
        return output;
    for (DWORD index = 0;; ++index)
    {
        wchar_t buffer[16384];
        DWORD size = 16384;
        const auto code = RegEnumValueW(key, index, buffer, &size, nullptr, nullptr, nullptr, nullptr);
        if (code == ERROR_NO_MORE_ITEMS)
            break;
        if (code != ERROR_SUCCESS)
            break;
        output.append(QString::fromWCharArray(buffer, size));
    }
    RegCloseKey(key);
    return output;
}

QString registry::resolveClass(shellAssoc& item) const
{
    QString current = item.progId;
    QSet<QString> seen;
    for (int depth = 0; depth < 8; ++depth)
    {
        if (current.isEmpty() || current.contains('\\') || current.contains('/') ||
            seen.contains(current.toLower()))
        {
            return QObject::tr("CurVer 类重定向无效或存在循环，已停止修改。");
        }
        seen.insert(current.toLower());
        const QString base = "Software\\Classes\\" + current;
        const regValue user = read("HKCU", base + "\\CurVer");
        const regValue machine = read("HKLM", base + "\\CurVer");
        for (const QString& root : {QString("HKCU"), QString("HKLM")})
        {
            item.context.append(read(root, base));
            item.context.append(read(root, base + "\\shell"));
            item.context.append(read(root, base + "\\shell\\open\\command", "DelegateExecute"));
            item.context.append(read(root, base + "\\shell\\open\\ddeexec"));
            item.context.append(read(root, base + "\\shell\\open\\DropTarget"));
            item.context.append(read(root, base + "\\shell\\open", "ExplorerCommandHandler"));
        }
        item.context.append(user);
        item.context.append(machine);
        const regValue next = user.exists ? user : machine;
        if (!next.exists || next.text().compare(current, Qt::CaseInsensitive) == 0)
        {
            item.classId = current;
            return {};
        }
        current = next.text();
    }
    return QObject::tr("CurVer 类重定向无效或存在循环，已停止修改。");
}

shellAssoc registry::query(const QString& fileExt) const
{
    shellAssoc result;
    result.fileExt = fileExt;
    result.progId = shellText(fileExt, ASSOCSTR_PROGID);
    if (result.progId.isEmpty() || result.progId.contains('\\') || result.progId.contains('/'))
    {
        result.issue = QObject::tr("未找到可确认的打开关联。请先在 Windows 默认应用中选择 Aspen Plus。");
        return result;
    }
    result.issue = resolveClass(result);
    if (result.issue.isEmpty())
    {
        result.command = classText(result.classId, ASSOCSTR_COMMAND);
        result.exePath = cleanPath(classText(result.classId, ASSOCSTR_EXECUTABLE));
    }
    for (const regValue& value : result.context)
    {
        if (!value.error.isEmpty())
        {
            result.issue = value.error;
            return result;
        }
        const bool subkey = value.path.endsWith("\\ddeexec", Qt::CaseInsensitive) ||
                            value.path.endsWith("\\DropTarget", Qt::CaseInsensitive);
        const bool hasHook = subkey ? value.keyExists : (!value.name.isEmpty() && value.exists);
        if (hasHook)
        {
            result.issue = QObject::tr(
                "关联含 DDE、COM 或无法读取的打开机制。仅改 EXE 不能保证指定版本，首版停止修改。");
            return result;
        }
    }
    if (!result.issue.isEmpty())
    {
        return result;
    }
    if (result.progId.isEmpty() || result.command.isEmpty() || result.progId.contains('\\') ||
        result.progId.contains('/'))
    {
        result.issue = QObject::tr("未找到可确认的打开关联。请先在 Windows 默认应用中选择 Aspen Plus。");
        return result;
    }
    if (!isAspen(result.exePath))
    {
        result.issue = QObject::tr("当前不是可验证的 Aspen Plus 程序。请先在 Windows 默认应用中选择 Aspen "
                                   "Plus；若路径失效，请修复安装。");
        return result;
    }
    const auto parsed = parseCommand(result.command);
    if (!parsed.valid || !samePath(parsed.exePath, result.exePath))
    {
        result.issue = QObject::tr("命令中的 EXE 与 Windows 实际解析的程序不一致，已停止修改。");
        return result;
    }
    if (!sameCommand(classText(result.classId, ASSOCSTR_COMMAND, false), result.command))
    {
        result.issue = QObject::tr("默认动作不是 open，首版无法安全修改。");
        return result;
    }
    const QString base = "Software\\Classes\\" + result.classId;
    const QString regPath = base + "\\shell\\open\\command";
    const auto user = read("HKCU", regPath);
    const auto machine = read("HKLM", regPath);
    if (!user.error.isEmpty() || !machine.error.isEmpty())
    {
        result.issue = user.error + machine.error;
        return result;
    }
    // A duplicate per-user class hides the machine class in the merged view.
    const auto classValue = read("HKCU", base);
    if (!classValue.error.isEmpty())
    {
        result.issue = classValue.error;
        return result;
    }
    const bool userClass = classValue.keyExists;
    result.source = userClass ? user : machine;
    if (!result.source.exists || !sameCommand(result.source.text(), result.command))
    {
        const QString absent = QObject::tr("（无默认命令）");
        result.issue = QObject::tr("无法确认实际命令的 HKCU/HKLM 来源（64 位视图），已停止修改。\n"
                                   "Windows 返回命令：%1\nHKCU 默认命令：%2\nHKLM 默认命令：%3\n"
                                   "HKCU 存在同名类：%4")
                           .arg(result.command, user.exists ? user.text() : absent,
                                machine.exists ? machine.text() : absent,
                                userClass ? QObject::tr("是") : QObject::tr("否"));
        return result;
    }
    QString error;
    replaceExe(result.command, result.exePath, error);
    result.issue = error;
    return result;
}

QString exeProduct(const QString& path)
{
    const auto file = path.toStdWString();
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(file.c_str(), &ignored);
    if (!size || size > 4 * 1024 * 1024)
        return {};
    QByteArray data(size, 0);
    if (!GetFileVersionInfoW(file.c_str(), 0, size, data.data()))
        return {};

    struct language
    {
        WORD lang;
        WORD code;
    };

    language* languages = nullptr;
    UINT count = 0;
    if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&languages),
                        &count))
        return {};
    QString output;
    for (UINT i = 0; i < count / sizeof(language); ++i)
    {
        for (const QString& field :
             {QString("ProductName"), QString("FileDescription"), QString("CompanyName")})
        {
            const auto key = QString("\\StringFileInfo\\%1%2\\%3")
                                 .arg(languages[i].lang, 4, 16, QChar('0'))
                                 .arg(languages[i].code, 4, 16, QChar('0'))
                                 .arg(field)
                                 .toStdWString();
            wchar_t* value = nullptr;
            UINT length = 0;
            if (VerQueryValueW(data.data(), key.c_str(), reinterpret_cast<void**>(&value), &length) && length)
                output += QString::fromWCharArray(value) + ' ';
        }
    }
    return output.trimmed();
}

bool registry::isAspen(const QString& exePath) const
{
    const auto name = QFileInfo(exePath).fileName();
    return fileExists(exePath) &&
           (name.compare("apwn.exe", Qt::CaseInsensitive) == 0 ||
            name.compare("aspenplus.exe", Qt::CaseInsensitive) == 0) &&
           exeProduct(exePath).contains("Aspen Plus", Qt::CaseInsensitive);
}

bool registry::fileExists(const QString& exePath) const
{
    return QFileInfo(exePath).isFile();
}

void registry::notify()
{
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSH, nullptr, nullptr);
}
