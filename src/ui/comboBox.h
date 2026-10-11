#pragma once

#include <QComboBox>

class theme;

class comboBox : public QComboBox
{
    Q_OBJECT

    public:
    explicit comboBox(theme& colors, QWidget* parent = nullptr);
    void showPopup() override;
    void hidePopup() override;
    QSize sizeHint() const override;

    protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

    private:
    bool expanded = false;
    static QRect placePopup(const QRect& anchor, const QRect& area, const QSize& size);
    void movePopup();
};
