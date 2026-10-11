#pragma once
#include "assoc/registry.h"
#include <QMap>

struct versionInfo
{
    QString name;
    QString exePath;
    QString installPath;
    QString origin;
    QString issue;
    bool valid = false;
    QMap<QString, QString> commands;
};

QString productLabel(const QString& text);
versionInfo inspectVersion(const QString& exePath, const QString& label, const registry& reg);
QList<versionInfo> scanVersions(const registry& reg, const QList<versionInfo>& manual = {});
QList<versionInfo> loadManual();
void saveManual(const QList<versionInfo>& versions);
