#pragma once
#include <QByteArray>
#include <QStringList>
#include <QList>

struct regValue
{
    QString root = QStringLiteral("HKCU");
    QString path;
    QString name;
    int view = 64;
    quint32 type = 0;
    QByteArray data;
    bool keyExists = false;
    bool exists = false;
    QString error;
    QString text() const;
    QString id() const;
};

regValue stringValue(regValue value, const QString& text);
bool sameValue(const regValue& a, const regValue& b);
QString exeProduct(const QString& path);

struct shellAssoc
{
    QString fileExt;
    QString progId;
    QString classId;
    QString command;
    QString exePath;
    QString issue;
    regValue source;
    QList<regValue> context;
};

// This seam also supplies the entirely in-memory test/demo backend.
class registry
{
    public:
    virtual ~registry() = default;
    virtual regValue read(const QString& root, const QString& path, const QString& name = {},
                          int view = 64) const;
    virtual QString write(const regValue& value);
    virtual QStringList keys(const QString& root, const QString& path, int view) const;
    virtual QStringList names(const QString& root, const QString& path, int view) const;
    virtual shellAssoc query(const QString& fileExt) const;
    virtual bool isAspen(const QString& exePath) const;
    virtual bool fileExists(const QString& exePath) const;
    virtual void notify();

    protected:
    virtual QString shellText(const QString& fileExt, int field, bool open = true) const;
    virtual QString classText(const QString& progId, int field, bool open = true) const;

    private:
    QString resolveClass(shellAssoc& item) const;
};
