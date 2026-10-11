#include "widgets.h"
#include "settings/theme.h"
#include "settings/language.h"
#include <QStyle>
#include <QPainter>
#include <QTextLayout>
#include <QDir>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QScrollArea>
#include "versions/versions.h"

namespace
{
qreal arrangeText(QTextLayout& layout, int width)
{
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);
    layout.beginLayout();
    qreal height = 0;
    while (true)
    {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(qMax(width, 1));
        line.setPosition(QPointF(0, height));
        height += line.height();
    }
    layout.endLayout();
    return height;
}
} // namespace

pathLabel::pathLabel(const QString& path, QWidget* parent) : QLabel(path, parent)
{
    setProperty("role", "muted");
    setTextFormat(Qt::PlainText);
    setMinimumWidth(0);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
}

void pathLabel::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setPen(palette().color(QPalette::WindowText));
    painter.drawText(contentsRect(), Qt::AlignLeft | Qt::AlignVCenter,
                     fontMetrics().elidedText(text(), Qt::ElideMiddle, contentsRect().width()));
}

actionButton::actionButton(const QString& text, bool primary, QWidget* parent) : QPushButton(text, parent)
{
    bindText(this, text);
    setProperty("role", primary ? "primary" : "button");
    setAutoDefault(false);
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(uiSize::controlHeight);
}

navButton::navButton(const QString& text, QWidget* parent) : QPushButton(text, parent)
{
    bindText(this, text);
    setProperty("role", "nav");
    setCheckable(true);
    setAutoExclusive(true);
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(uiSize::controlHeight + uiSize::space8);
}

statusTag::statusTag(const QString& text, const QString& tone, QWidget* parent) : QLabel(text, parent)
{
    bindText(this, text);
    setProperty("role", "badge");
    setProperty("tone", tone);
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
}

card::card(QWidget* parent) : QFrame(parent), body(new QVBoxLayout(this))
{
    setProperty("role", "card");
    body->setContentsMargins(uiSize::space16, uiSize::space12, uiSize::space16, uiSize::space12);
    body->setSpacing(uiSize::space8);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
}

void card::setSelected(bool selected)
{
    setProperty("selected", selected);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

versionRow::versionRow(const versionInfo& item, bool selected, bool writable, QWidget* parent) : card(parent)
{
    setObjectName("versionRow");
    setSelected(selected);

    QLabel* nameLabel = new QLabel(item.name, this);
    nameLabel->setProperty("role", "heading");
    nameLabel->setTextFormat(Qt::PlainText);
    nameLabel->setWordWrap(true);
    nameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    pathLabel* location = new pathLabel(QDir::toNativeSeparators(item.installPath), this);
    location->setObjectName("versionPath");
    actionButton* moreBtn = new actionButton("安装信息", false, this);
    moreBtn->setObjectName("versionInfoBtn");
    moreBtn->setProperty("role", "quiet");
    moreBtn->setCheckable(true);

    QVBoxLayout* infoLayout = new QVBoxLayout;
    infoLayout->setSpacing(uiSize::space4);
    infoLayout->setAlignment(Qt::AlignTop);
    infoLayout->addWidget(nameLabel);
    infoLayout->addWidget(location);
    if (!item.issue.isEmpty())
    {
        QLabel* issueLabel = textLabel(item.issue, "warning", this);
        infoLayout->addWidget(issueLabel);
    }

    QHBoxLayout* rowLayout = new QHBoxLayout;
    rowLayout->setSpacing(uiSize::space12);
    rowLayout->addLayout(infoLayout, 1);
    rowLayout->addWidget(moreBtn);
    if (selected)
    {
        statusTag* state = new statusTag("当前默认", "selected", this);
        state->setObjectName("defaultTag");
        rowLayout->addWidget(state, 0, Qt::AlignVCenter);
    }
    else
    {
        actionButton* useBtn = new actionButton("设为默认", true, this);
        useBtn->setObjectName("applyBtn");
        useBtn->setProperty("available", item.valid && writable);
        useBtn->setEnabled(item.valid && writable);
        rowLayout->addWidget(useBtn);
        connect(useBtn, &QPushButton::clicked, this, &versionRow::useRequested);
    }

    QPlainTextEdit* infoText = new QPlainTextEdit(this);
    infoText->setObjectName("versionInfoText");
    infoText->setReadOnly(true);
    infoText->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    infoText->setPlainText(QObject::tr("安装目录：%1\n程序路径：%2\n来源：%3")
                               .arg(QDir::toNativeSeparators(item.installPath),
                                    QDir::toNativeSeparators(item.exePath), item.origin));
    infoText->setMinimumHeight(100);
    infoText->setMaximumHeight(140);
    infoText->hide();

    body->addLayout(rowLayout);
    body->addWidget(infoText);

    connect(moreBtn, &QPushButton::toggled, this,
            [moreBtn, infoText](bool expanded)
            {
                infoText->setVisible(expanded);
                bindText(moreBtn, expanded ? "收起信息" : "安装信息");
            });
}

settingRow::settingRow(const QString& title, const QString& description, QWidget* control, QWidget* parent)
    : QFrame(parent)
{
    setProperty("role", "settingRow");
    control->setFixedWidth(uiSize::selectWidth);

    QLabel* titleLabel = textLabel(title, {}, this);

    textLayout = new QVBoxLayout;
    textLayout->setSpacing(uiSize::space4);
    textLayout->addWidget(titleLabel, 0, description.isEmpty() ? Qt::AlignVCenter : Qt::Alignment{});
    if (!description.isEmpty())
    {
        textLayout->addWidget(textLabel(description, "muted", this));
    }

    QHBoxLayout* rowLayout = new QHBoxLayout(this);
    rowLayout->setContentsMargins(0, uiSize::space12, 0, uiSize::space12);
    rowLayout->setSpacing(uiSize::space24);
    rowLayout->addLayout(textLayout, 1);
    rowLayout->addWidget(control, 0, Qt::AlignVCenter);
}

wrapLabel::wrapLabel(const QString& text, QWidget* parent) : QLabel(text, parent)
{
    setWordWrap(true);
    setTextFormat(Qt::PlainText);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setMinimumWidth(0);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
}

int wrapLabel::heightForWidth(int width) const
{
    QTextLayout layout(text().replace('\n', QChar::LineSeparator), font());
    return qCeil(arrangeText(layout, width - contentsMargins().left() - contentsMargins().right())) +
           contentsMargins().top() + contentsMargins().bottom();
}

QSize wrapLabel::sizeHint() const
{
    return QSize(400, heightForWidth(400));
}

QSize wrapLabel::minimumSizeHint() const
{
    return QSize(0, fontMetrics().height());
}

void wrapLabel::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setPen(
        palette().color(isEnabled() ? QPalette::Normal : QPalette::Disabled, QPalette::WindowText));
    QTextLayout layout(text().replace('\n', QChar::LineSeparator), font());
    arrangeText(layout, contentsRect().width());
    layout.draw(&painter, contentsRect().topLeft());
}

QLabel* textLabel(const QString& text, const QString& role, QWidget* parent)
{
    wrapLabel* label = new wrapLabel(text, parent);
    if (!text.isEmpty())
    {
        bindText(label, text);
    }
    label->setProperty("role", role);
    return label;
}

QScrollArea* scrollPage(QWidget* body)
{
    QScrollArea* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(body);
    return scroll;
}
