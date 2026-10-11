#pragma once
#include "assoc/registry.h"
#include <QList>

struct regChange
{
    regValue before;
    regValue after;
};

struct assocBackup
{
    QString state;
    QString created;
    QList<regChange> changes;
    QList<regValue> context;
    QList<shellAssoc> prior;
};

QString saveBackup(const QString& path, const assocBackup& backup);
bool loadBackup(const QString& path, assocBackup& backup, QString& error);
QString backupDir();
QStringList listBackups();
