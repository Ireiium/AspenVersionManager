#include <QObject>
#include "versions.h"
#include "assoc/command.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QSet>

QString productLabel(const QString& text)
{
    static const QRegularExpression pattern(R"(Aspen\s*Plus[\s_\\/\-]*(?:V|Version\s*)?(\d+(?:\.\d+)?))",
                                            QRegularExpression::CaseInsensitiveOption);
    const auto match = pattern.match(text);
    return match.hasMatch() ? "Aspen Plus V" + match.captured(1) : QObject::tr("Aspen Plus（版本待确认）");
}

versionInfo inspectVersion(const QString& exePath, const QString& label, const registry& reg)
{
    versionInfo item;
    item.exePath = cleanPath(exePath);
    item.installPath = QFileInfo(item.exePath).absolutePath();
    item.name = label.trimmed().isEmpty() ? productLabel(item.exePath) : label.trimmed();
    item.valid = reg.isAspen(item.exePath);
    if (!item.valid)
        item.issue = reg.fileExists(item.exePath)
                         ? QObject::tr("程序身份未确认：需要 Aspen Plus 的 apwn.exe 或 "
                                       "aspenplus.exe，且产品信息包含 Aspen Plus。")
                         : QObject::tr("程序路径不存在");
    return item;
}

QList<versionInfo> scanVersions(const registry& reg, const QList<versionInfo>& manual)
{
    QList<versionInfo> output;
    auto add = [&](versionInfo item)
    {
        for (auto& old : output)
        {
            if (samePath(old.exePath, item.exePath))
            {
                if (old.name == productLabel({}) && item.name != productLabel({}))
                    old.name = item.name;
                for (auto it = item.commands.begin(); it != item.commands.end(); ++it)
                    old.commands[it.key()] = it.value();
                return;
            }
        }
        output.append(item);
    };
    for (const auto& item : manual)
    {
        auto checked = inspectVersion(item.exePath, item.name, reg);
        checked.origin = QObject::tr("手动添加");
        add(checked);
    }
    QStringList roots;
    QMap<QString, QString> labels;
    const QString uninstall = "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
    for (const QString& root : {QString("HKLM"), QString("HKCU")})
    {
        for (int view : {64, 32})
        {
            for (const auto& key : reg.keys(root, uninstall, view))
            {
                const QString path = uninstall + '\\' + key;
                const QString name = reg.read(root, path, "DisplayName", view).text();
                if (!name.contains("Aspen Plus", Qt::CaseInsensitive))
                    continue;
                const QString location = cleanPath(reg.read(root, path, "InstallLocation", view).text());
                if (!location.isEmpty() && location != "." && QFileInfo(location).isAbsolute())
                {
                    roots.append(location);
                    labels[location] = productLabel(name);
                }
                const auto icon =
                    parseCommand(reg.read(root, path, "DisplayIcon", view).text().section(',', 0, 0));
                if (icon.valid &&
                    (QFileInfo(icon.exePath).fileName().compare("apwn.exe", Qt::CaseInsensitive) == 0 ||
                     QFileInfo(icon.exePath).fileName().compare("aspenplus.exe", Qt::CaseInsensitive) == 0))
                {
                    auto item = inspectVersion(icon.exePath, productLabel(name), reg);
                    item.origin = QObject::tr("安装注册信息");
                    add(item);
                }
            }
        }
    }
    // Enumerate only Aspen-like classes and the active extension mapping, never whole disks.
    QSet<QString> progIds;
    QMap<QString, QSet<QString>> formats;
    for (const QString& ext : {QString(".apw"), QString(".apwz"), QString(".bkp")})
    {
        const auto active = reg.query(ext);
        if (!active.progId.isEmpty())
        {
            progIds.insert(active.progId);
            formats[active.progId.toLower()].insert(ext);
        }
        if (reg.isAspen(active.exePath))
        {
            auto item = inspectVersion(active.exePath, {}, reg);
            item.origin = QObject::tr("当前文件关联");
            item.commands[ext] = active.command;
            add(item);
        }
        for (const QString& root : {QString("HKCU"), QString("HKLM")})
            for (int view : {64, 32})
            {
                const QString extPath = "Software\\Classes\\" + ext;
                const QString id = reg.read(root, extPath, {}, view).text();
                if (!id.isEmpty())
                {
                    progIds.insert(id);
                    formats[id.toLower()].insert(ext);
                }
                for (const auto& name : reg.names(root, extPath + "\\OpenWithProgids", view))
                {
                    if (!name.isEmpty())
                    {
                        progIds.insert(name);
                        formats[name.toLower()].insert(ext);
                    }
                }
            }
    }
    for (const QString& root : {QString("HKCU"), QString("HKLM")})
    {
        for (int view : {64, 32})
        {
            for (const auto& key : reg.keys(root, "Software\\Classes", view))
                if (key.contains("aspen", Qt::CaseInsensitive) || key.contains("apw", Qt::CaseInsensitive))
                    progIds.insert(key);
            for (const auto& id : progIds)
            {
                if (id.contains('\\') || id.contains('/'))
                    continue;
                const auto command =
                    reg.read(root, "Software\\Classes\\" + id + "\\shell\\open\\command", {}, view).text();
                const auto parsed = parseCommand(command);
                if (!parsed.valid || !reg.isAspen(parsed.exePath))
                    continue;
                auto item = inspectVersion(parsed.exePath, {}, reg);
                item.origin = QObject::tr("文件类型注册信息");
                // A class name alone is not enough to infer which format its arguments belong to.
                for (const auto& ext : formats.value(id.toLower()))
                    item.commands[ext] = command;
                add(item);
                QDir parent(QFileInfo(parsed.exePath).absolutePath());
                for (int depth = 0; depth < 4; ++depth)
                {
                    if (parent.dirName().contains("Aspen Plus", Qt::CaseInsensitive))
                    {
                        const auto sibling = QFileInfo(parent.absolutePath()).dir();
                        for (const auto& entry : sibling.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
                            if (entry.fileName().contains("Aspen Plus", Qt::CaseInsensitive))
                                roots.append(entry.absoluteFilePath());
                        roots.append(parent.absolutePath());
                        break;
                    }
                    if (!parent.cdUp())
                        break;
                }
            }
        }
    }
    roots.removeDuplicates();
    for (const auto& root : roots)
    {
        QList<QPair<QString, int>> pending{{root, 0}};
        int count = 0;
        while (!pending.isEmpty() && count++ < 1500)
        {
            const auto current = pending.takeFirst();
            QDir dir(current.first);
            for (const auto& file : dir.entryInfoList({"apwn.exe", "aspenplus.exe"}, QDir::Files))
            {
                auto item = inspectVersion(file.absoluteFilePath(),
                                           labels.value(root, productLabel(file.absoluteFilePath())), reg);
                item.installPath = root;
                item.origin = QObject::tr("安装目录");
                add(item);
            }
            if (current.second >= 5)
                continue;
            for (const auto& sub : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks))
                pending.append({sub.absoluteFilePath(), current.second + 1});
        }
    }
    std::sort(output.begin(), output.end(), [](const auto& a, const auto& b) { return a.name > b.name; });
    return output;
}

QList<versionInfo> loadManual()
{
    QSettings settings;
    QList<versionInfo> output;
    const int size = settings.beginReadArray("manualVersions");
    for (int i = 0; i < size; ++i)
    {
        settings.setArrayIndex(i);
        versionInfo item;
        item.name = settings.value("name").toString();
        item.exePath = settings.value("exePath").toString();
        output.append(item);
    }
    settings.endArray();
    return output;
}

void saveManual(const QList<versionInfo>& versions)
{
    QSettings settings;
    settings.beginWriteArray("manualVersions", versions.size());
    for (int i = 0; i < versions.size(); ++i)
    {
        settings.setArrayIndex(i);
        settings.setValue("name", versions[i].name);
        settings.setValue("exePath", versions[i].exePath);
    }
    settings.endArray();
    settings.sync();
}
