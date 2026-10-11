#include <QObject>
#include "associations.h"
#include "command.h"
#include <QDateTime>
#include <QDir>
#include <QUuid>
#include <QSet>
#include <QThread>

associations::associations(registry& reg, QString folder) : reg(reg), folder(std::move(folder)) {}

QList<shellAssoc> associations::query() const
{
    QList<shellAssoc> output;
    for (const QString& ext : {QString(".apw"), QString(".apwz"), QString(".bkp")})
        output.append(reg.query(ext));
    return output;
}

QString associations::checkContext(const QList<regValue>& context) const
{
    for (const auto& value : context)
    {
        const auto now = reg.read(value.root, value.path, value.name, value.view);
        if (!now.error.isEmpty())
            return now.error;
        if (!sameValue(value, now))
            return QObject::tr("关联上下文发生变化，请刷新后重试：%1\\%2").arg(value.root, value.path);
    }
    return {};
}

assocResult associations::applyAssoc(const versionInfo& version)
{
    if (!reg.fileExists(version.exePath) || !reg.isAspen(version.exePath))
        return {false, QObject::tr("目标程序不存在或不是可验证的 Aspen Plus，请重新添加。"), {}, {}};
    assocBackup backup;
    backup.prior = query();
    QSet<QString> ids;
    for (const auto& item : backup.prior)
    {
        if (!item.issue.isEmpty() || !item.source.exists)
            return {false, item.fileExt + QObject::tr(" 无法安全修改。"), item.issue, {}};
        QString error;
        const QString source = version.commands.value(item.fileExt, item.command);
        const QString newCmd = replaceExe(source, version.exePath, error);
        if (!error.isEmpty())
            return {false, item.fileExt + QObject::tr(" 打开命令无法安全修改。"), error, {}};
        const auto after = stringValue(item.source, newCmd);
        if (ids.contains(item.source.id()))
        {
            for (const auto& old : backup.changes)
                if (old.before.id() == item.source.id() && old.after.data != after.data)
                    return {
                        false, QObject::tr("多个扩展名共享 ProgID，但需要不同参数，已停止修改。"), {}, {}};
        }
        else
        {
            backup.changes.append({item.source, after});
            ids.insert(item.source.id());
        }
        const QString extPath = "Software\\Classes\\" + item.fileExt;
        const QString choice = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" +
                               item.fileExt + "\\UserChoice";
        backup.context.append(reg.read("HKCU", choice, "ProgId"));
        backup.context.append(reg.read("HKCU", choice, "Hash"));
        backup.context.append(item.context);
        for (const QString& root : {QString("HKCU"), QString("HKLM")})
        {
            backup.context.append(reg.read(root, extPath));
            const QString base = "Software\\Classes\\" + item.progId;
            backup.context.append(reg.read(root, base));
            backup.context.append(reg.read(root, base + "\\CurVer"));
            backup.context.append(reg.read(root, base + "\\shell"));
            backup.context.append(reg.read(root, base + "\\shell\\open\\command", "DelegateExecute"));
            backup.context.append(reg.read(root, base + "\\shell\\open\\ddeexec"));
            backup.context.append(reg.read(root, base + "\\shell\\open\\DropTarget"));
            backup.context.append(reg.read(root, base + "\\shell\\open", "ExplorerCommandHandler"));
        }
    }
    for (const auto& item : backup.context)
        if (!item.error.isEmpty())
            return {false, QObject::tr("无法完整读取关联上下文，未修改。"), item.error, {}};
    return commit(backup, false);
}

QString associations::checkResult(const assocBackup& backup) const
{
    const QString contextError = checkContext(backup.context);
    if (!contextError.isEmpty())
    {
        return contextError;
    }

    for (const regChange& item : backup.changes)
    {
        const regValue written = reg.read(item.after.root, item.after.path, item.after.name, item.after.view);
        if (!sameValue(item.after, written))
        {
            return QObject::tr("写入后的注册表命令发生变化：%1\\%2\n%3")
                .arg(item.after.root, item.after.path, written.error);
        }
    }

    for (const shellAssoc& prior : backup.prior)
    {
        const shellAssoc now = reg.query(prior.fileExt);
        QString expected;
        for (const regChange& item : backup.changes)
        {
            if (item.before.id() == prior.source.id())
            {
                expected = item.after.text();
                break;
            }
        }

        const openCommand parsed = parseCommand(expandEnv(expected));
        if (now.progId.compare(prior.progId, Qt::CaseInsensitive) != 0 ||
            !sameCommand(now.command, expected) || !samePath(now.exePath, parsed.exePath) ||
            now.source.id() != prior.source.id() || !now.issue.isEmpty())
        {
            return QObject::tr("%1：Windows 实际生效结果与预期不一致。\n"
                               "预期 ProgID：%2\n实际 ProgID：%3\n预期命令：%4\n"
                               "Windows 返回命令：%5\nWindows 返回程序：%6\n%7")
                .arg(prior.fileExt, prior.progId, now.progId, expected, now.command, now.exePath, now.issue);
        }
    }
    return {};
}

assocResult associations::commit(assocBackup backup, bool restoring)
{
    const QString contextError = checkContext(backup.context);
    if (!contextError.isEmpty())
        return {false, QObject::tr("关联在操作前已发生变化，未修改。"), contextError, {}};
    for (const auto& item : backup.changes)
    {
        const auto now = reg.read(item.before.root, item.before.path, item.before.name, item.before.view);
        if (!now.error.isEmpty() || !sameValue(item.before, now))
            return {false, QObject::tr("打开命令在操作前已变化，请刷新后重试。"), now.error, {}};
    }
    backup.state = "pending";
    backup.created = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    const QString path = folder + '/' + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz") +
                         '-' + QUuid::createUuid().toString(QUuid::Id128) + ".json";
    const auto saveError = saveBackup(path, backup);
    if (!saveError.isEmpty())
        return {false, QObject::tr("备份失败，未修改文件关联。"), saveError, path};
    QList<regChange> touched;
    QString failure;
    for (const auto& item : backup.changes)
    {
        failure = checkContext(backup.context);
        if (!failure.isEmpty())
            break;
        const auto now = reg.read(item.before.root, item.before.path, item.before.name, item.before.view);
        if (!sameValue(now, item.before) || !now.error.isEmpty())
        {
            failure = QObject::tr("写入前命令发生变化。");
            break;
        }
        // Include a possibly flushed-but-failed write in rollback as well.
        touched.append(item);
        failure = reg.write(item.after);
        if (!failure.isEmpty())
        {
            failure = item.before.root + '\\' + item.before.path + '\n' + failure;
            break;
        }
        const auto written = reg.read(item.after.root, item.after.path, item.after.name, item.after.view);
        if (!written.error.isEmpty() || !sameValue(item.after, written))
        {
            failure = QObject::tr("注册表写入后读取验证失败。");
            break;
        }
    }
    reg.notify();
    if (failure.isEmpty())
    {
        // Notifications can precede fresh Shell results. Retry only reads, on the existing worker task.
        for (const unsigned long delay : {0UL, 50UL, 100UL, 200UL, 400UL})
        {
            if (delay)
            {
                QThread::msleep(delay);
            }
            failure = checkResult(backup);
            if (failure.isEmpty())
            {
                break;
            }
        }
    }
    if (failure.isEmpty())
    {
        backup.state = restoring ? "restored" : "applied";
        const auto finalError = saveBackup(path, backup);
        if (finalError.isEmpty())
            return {true,
                    restoring ? QObject::tr("已恢复并验证原有关联。")
                              : QObject::tr("已更新并验证三个文件类型的实际关联。"),
                    {},
                    path};
        failure = QObject::tr("最终备份状态无法保存，执行回滚。\n") + finalError;
    }
    QStringList rollbackErrors;
    for (auto it = touched.crbegin(); it != touched.crend(); ++it)
    {
        const auto now = reg.read(it->after.root, it->after.path, it->after.name, it->after.view);
        if (sameValue(now, it->before))
            continue;
        if (!sameValue(now, it->after))
        {
            rollbackErrors.append(it->before.path + QObject::tr("：命令被外部改变，未覆盖。"));
            continue;
        }
        const auto error = reg.write(it->before);
        const auto check = reg.read(it->before.root, it->before.path, it->before.name, it->before.view);
        if (!error.isEmpty() || !sameValue(check, it->before))
            rollbackErrors.append(it->before.path + '\n' + error);
    }
    reg.notify();
    for (const auto& prior : backup.prior)
    {
        const auto now = reg.query(prior.fileExt);
        if (now.progId.compare(prior.progId, Qt::CaseInsensitive) != 0 ||
            !sameCommand(now.command, prior.command) || now.source.id() != prior.source.id() ||
            !now.issue.isEmpty())
            rollbackErrors.append(prior.fileExt + QObject::tr("：回滚后实际关联已变化，请检查默认应用。"));
    }
    backup.state = rollbackErrors.isEmpty() ? "rolledBack" : "recoveryNeeded";
    const auto finalError = saveBackup(path, backup);
    if (!finalError.isEmpty())
        rollbackErrors.append(finalError);
    return {false,
            rollbackErrors.isEmpty() ? QObject::tr("更新失败，本次修改已恢复。")
                                     : QObject::tr("更新失败，部分项目需要人工恢复。"),
            failure + (rollbackErrors.isEmpty() ? QString()
                                                : QObject::tr("\n回滚详情：\n") + rollbackErrors.join('\n')),
            path};
}

assocResult associations::restoreAssoc(const QString& path)
{
    assocBackup saved;
    QString error;
    if (!loadBackup(path, saved, error))
        return {false, QObject::tr("无法读取备份。"), error, path};
    error = checkContext(saved.context);
    if (!error.isEmpty())
        return {false, QObject::tr("Windows 默认应用或关联元数据已变化，停止恢复以免覆盖新配置。"), error,
                path};
    assocBackup restore;
    restore.context = saved.context;
    restore.prior = query();
    for (const auto& item : restore.prior)
        if (!item.issue.isEmpty())
            return {false, item.fileExt + QObject::tr(" 当前关联无法安全恢复。"), item.issue, path};
    QSet<QString> ids;
    for (const auto& item : saved.changes)
    {
        if (ids.contains(item.before.id()))
            return {false, QObject::tr("备份包含重复写入项。"), {}, path};
        ids.insert(item.before.id());
        bool matched = false;
        for (const auto& current : restore.prior)
            if (current.source.id() == item.before.id())
                matched = true;
        if (!matched)
            return {false, QObject::tr("备份的命令来源已经变化，停止恢复。"), {}, path};
        const auto parsed = parseCommand(item.before.text());
        if (!parsed.valid || !reg.isAspen(parsed.exePath))
            return {false, QObject::tr("备份中的 Aspen 程序已失效，请修复安装后恢复。"), item.before.text(),
                    path};
        const auto now = reg.read(item.before.root, item.before.path, item.before.name, item.before.view);
        if (!sameValue(now, item.after) && !sameValue(now, item.before))
            return {false, QObject::tr("关联已被其他工具修改，停止恢复。"), item.before.path, path};
        restore.changes.append({now, item.before});
    }
    return commit(restore, true);
}
