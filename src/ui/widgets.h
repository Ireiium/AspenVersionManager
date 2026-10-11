#pragma once
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

struct versionInfo;
class QScrollArea;

class pathLabel : public QLabel
{
    public:
    explicit pathLabel(const QString& path, QWidget* parent = nullptr);

    protected:
    void paintEvent(QPaintEvent* event) override;
};

class actionButton : public QPushButton
{
    public:
    actionButton(const QString& text, bool primary = false, QWidget* parent = nullptr);
};

class navButton : public QPushButton
{
    public:
    explicit navButton(const QString& text, QWidget* parent = nullptr);
};

class statusTag : public QLabel
{
    public:
    statusTag(const QString& text, const QString& tone, QWidget* parent = nullptr);
};

class card : public QFrame
{
    public:
    explicit card(QWidget* parent = nullptr);
    QVBoxLayout* body;
    void setSelected(bool selected);
};

class versionRow : public card
{
    Q_OBJECT

    public:
    versionRow(const versionInfo& item, bool selected, bool writable, QWidget* parent = nullptr);

    signals:
    void useRequested();
};

class settingRow : public QFrame
{
    public:
    settingRow(const QString& title, const QString& description, QWidget* control, QWidget* parent = nullptr);
    QVBoxLayout* textLayout;
};

class wrapLabel : public QLabel
{
    public:
    explicit wrapLabel(const QString& text, QWidget* parent = nullptr);
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    protected:
    void paintEvent(QPaintEvent* event) override;
};

QLabel* textLabel(const QString& text, const QString& role = {}, QWidget* parent = nullptr);
QScrollArea* scrollPage(QWidget* body);
