#pragma once
#include "settings/backup.h"
#include "versions/versions.h"

struct assocResult
{
    bool ok = false;
    QString message;
    QString details;
    QString backupPath;
};

class associations
{
    public:
    explicit associations(registry& reg, QString folder);
    QList<shellAssoc> query() const;
    assocResult applyAssoc(const versionInfo& version);
    assocResult restoreAssoc(const QString& path);

    private:
    registry& reg;
    QString folder;
    QString checkContext(const QList<regValue>& context) const;
    QString checkResult(const assocBackup& backup) const;
    assocResult commit(assocBackup backup, bool restoring);
};
