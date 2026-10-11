#include <QObject>
#include "backup.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QRegularExpression>

namespace
{
QJsonObject toJson(const regValue& value)
{
    return {{"root", value.root},           {"path", value.path},
            {"name", value.name},           {"view", value.view},
            {"type", qint64(value.type)},   {"data", QString::fromLatin1(value.data.toBase64())},
            {"keyExists", value.keyExists}, {"exists", value.exists}};
}

regValue fromJson(const QJsonObject& obj)
{
    regValue value;
    value.root = obj["root"].toString();
    value.path = obj["path"].toString();
    value.name = obj["name"].toString();
    value.view = obj["view"].toInt();
    value.type = obj["type"].toInteger();
    value.data = QByteArray::fromBase64(obj["data"].toString().toLatin1());
    value.exists = obj["exists"].toBool();
    value.keyExists = obj["keyExists"].toBool();
    return value;
}
} // namespace

QString backupDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/backups";
}

QStringList listBackups()
{
    QDir dir(backupDir());
    QStringList paths;
    for (const auto& name : dir.entryList({"*.json"}, QDir::Files, QDir::Name | QDir::Reversed))
        paths.append(dir.absoluteFilePath(name));
    return paths;
}

QString saveBackup(const QString& path, const assocBackup& backup)
{
    QJsonArray changes, context, prior;
    for (const auto& item : backup.changes)
        changes.append(QJsonObject{{"before", toJson(item.before)}, {"after", toJson(item.after)}});
    for (const auto& item : backup.context)
        context.append(toJson(item));
    for (const auto& item : backup.prior)
        prior.append(QJsonObject{{"ext", item.fileExt},
                                 {"progId", item.progId},
                                 {"command", item.command},
                                 {"exePath", item.exePath}});
    const QJsonObject obj{{"format", 1},        {"state", backup.state}, {"created", backup.created},
                          {"changes", changes}, {"context", context},    {"prior", prior}};
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return QObject::tr("无法创建备份目录。");
    QSaveFile file(path);
    const QByteArray data = QJsonDocument(obj).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
        return QObject::tr("无法原子保存备份：%1").arg(file.errorString());
    return {};
}

bool loadBackup(const QString& path, assocBackup& backup, QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
    {
        error = QObject::tr("备份文件无法读取或过大。");
        return false;
    }
    QJsonParseError parse;
    const auto obj = QJsonDocument::fromJson(file.readAll(), &parse).object();
    if (parse.error != QJsonParseError::NoError || obj["format"].toInt() != 1)
    {
        error = QObject::tr("备份格式无效。");
        return false;
    }
    backup = {};
    backup.state = obj["state"].toString();
    backup.created = obj["created"].toString();
    static const QRegularExpression allowed(R"(^Software\\Classes\\[^\\]+\\shell\\open\\command$)",
                                            QRegularExpression::CaseInsensitiveOption);
    for (const auto& entry : obj["changes"].toArray())
    {
        const auto data = entry.toObject();
        regChange item{fromJson(data["before"].toObject()), fromJson(data["after"].toObject())};
        if (item.before.id() != item.after.id() || !allowed.match(item.before.path).hasMatch() ||
            (item.before.root != "HKCU" && item.before.root != "HKLM") || !item.before.name.isEmpty() ||
            (item.before.view != 64 && item.before.view != 32) || !item.before.exists ||
            !item.before.keyExists || !item.after.exists || !item.after.keyExists ||
            (item.before.type != 1 && item.before.type != 2) || item.after.type != item.before.type ||
            item.before.text().isEmpty() || item.after.text().isEmpty())
        {
            error = QObject::tr("备份包含非现有打开命令，拒绝恢复。");
            return false;
        }
        backup.changes.append(item);
    }
    for (const auto& entry : obj["context"].toArray())
    {
        const auto value = fromJson(entry.toObject());
        if ((value.root != "HKCU" && value.root != "HKLM") || (value.view != 32 && value.view != 64))
        {
            error = QObject::tr("备份上下文无效。");
            return false;
        }
        backup.context.append(value);
    }
    for (const auto& entry : obj["prior"].toArray())
    {
        const auto value = entry.toObject();
        shellAssoc item;
        item.fileExt = value["ext"].toString();
        item.progId = value["progId"].toString();
        item.command = value["command"].toString();
        item.exePath = value["exePath"].toString();
        backup.prior.append(item);
    }
    if (backup.changes.isEmpty() || backup.changes.size() > 3 || backup.prior.size() != 3)
    {
        error = QObject::tr("备份缺少完整的三个文件类型记录。");
        return false;
    }
    return true;
}
