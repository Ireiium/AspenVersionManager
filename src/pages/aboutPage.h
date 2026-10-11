#pragma once

#include <QWidget>

class theme;
class QVBoxLayout;
class QUrl;

class aboutPage : public QWidget
{
    Q_OBJECT

    public:
    explicit aboutPage(theme& colors, QWidget* parent = nullptr);
    static QString rootPath(const QString& appDir);

    private:
    theme& colors;
    QString root;
    void initHeader(QVBoxLayout* body);
    void initCredits(QVBoxLayout* body);
    void initSource(QVBoxLayout* body);
    void addEntry(QVBoxLayout* body, const QString& name, const QUrl& url, const QString& license,
                  const QString& note);
    void openWeb(const QUrl& url);

    private slots:
    void openMit();
    void openLicenses();
};
