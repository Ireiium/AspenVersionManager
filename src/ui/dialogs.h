#pragma once

#include <QDialog>
#include <QFileDialog>
#include "assoc/registry.h"

class QVBoxLayout;
class QHBoxLayout;
class QTextBrowser;
class actionButton;
class QLabel;

class appDialog : public QDialog
{
    Q_OBJECT

    public:
    explicit appDialog(const QString& title, QWidget* parent = nullptr);
    QVBoxLayout* body;
    QHBoxLayout* footer;
    actionButton* addButtons(const QString& acceptText, bool cancel = true);
};

class adminDialog : public appDialog
{
    public:
    adminDialog();
    void setError(const QString& text);
    static constexpr int continueApp = 2;

    private:
    QLabel* notice;
};

class detailDialog : public appDialog
{
    Q_OBJECT

    public:
    explicit detailDialog(QWidget* parent = nullptr);
    void setAssocs(const QList<shellAssoc>& assocs, const QString& info = {});

    protected:
    void changeEvent(QEvent* event) override;

    private:
    QTextBrowser* content;
    QList<shellAssoc> items;
    QString operation;
    bool formatted = false;
};

class fileDialog : public QFileDialog
{
    public:
    fileDialog(const QString& title, bool folder, QWidget* parent = nullptr);

    protected:
    void changeEvent(QEvent* event) override;
};

bool showMessage(QWidget* parent, const QString& title, const QString& text, const QString& details = {},
                 bool error = false);
