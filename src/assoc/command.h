#pragma once
#include <QString>

struct openCommand
{
    QString exePath;
    QString args;
    bool valid = false;
};

QString expandEnv(const QString& text);
QString cleanPath(const QString& path);
bool samePath(const QString& left, const QString& right);
bool sameCommand(const QString& left, const QString& right);
openCommand parseCommand(const QString& text);
QString replaceExe(const QString& text, const QString& exePath, QString& error);
