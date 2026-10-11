#include "comboBox.h"
#include "settings/theme.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QFrame>
#include <QGuiApplication>
#include <QLayout>
#include <QListView>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QStyledItemDelegate>
#include <QStyleOptionComboBox>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QEvent>

namespace
{
class comboStyle : public QProxyStyle
{
    public:
    comboStyle() : QProxyStyle(QStyleFactory::create("Fusion")) {}

    int styleHint(StyleHint hint, const QStyleOption* option = nullptr, const QWidget* widget = nullptr,
                  QStyleHintReturn* data = nullptr) const override
    {
        // Use the list scrollbar rather than Qt's menu-style scroller arrows.
        if (hint == QStyle::SH_ComboBox_Popup)
        {
            return false;
        }
        if (hint == QStyle::SH_ComboBox_PopupFrameStyle)
        {
            return QFrame::NoFrame;
        }
        return QProxyStyle::styleHint(hint, option, widget, data);
    }
};

class comboItem : public QStyledItemDelegate
{
    public:
    explicit comboItem(comboBox* parent) : QStyledItemDelegate(parent), owner(parent) {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(qMax(uiSize::optionHeight, option.fontMetrics.height() + uiSize::space16));
        return size;
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const themeColors& color = theme::tokens();
        const bool chosen = index.row() == owner->currentIndex();
        const bool enabled = index.flags().testFlag(Qt::ItemIsEnabled);
        const bool active =
            option.state.testFlag(QStyle::State_Selected) || option.state.testFlag(QStyle::State_MouseOver);
        QRectF area = option.rect.adjusted(2, 2, -2, -2);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        if (chosen || active)
        {
            painter->setBrush(chosen ? color.selected : color.hover);
            painter->drawRoundedRect(area, uiSize::controlRadius, uiSize::controlRadius);
        }
        painter->setPen(enabled ? (chosen ? color.selectedText : color.text) : color.disabled);
        painter->setFont(option.font);
        QRect textArea = area.toRect().adjusted(uiSize::space12, 0, -uiSize::space24, 0);
        QString text =
            option.fontMetrics.elidedText(index.data().toString(), Qt::ElideRight, textArea.width());
        painter->drawText(textArea, Qt::AlignVCenter | Qt::AlignLeft, text);
        if (chosen)
        {
            QPointF point(area.right() - uiSize::space16, area.center().y());
            QPainterPath mark;
            mark.moveTo(point + QPointF(-4, 0));
            mark.lineTo(point + QPointF(-1, 3));
            mark.lineTo(point + QPointF(5, -4));
            painter->setPen(QPen(color.focus, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPath(mark);
        }
        if (active && owner->view()->hasFocus())
        {
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(color.focus, 1));
            painter->drawRoundedRect(area, uiSize::controlRadius, uiSize::controlRadius);
        }
        painter->restore();
    }

    private:
    comboBox* owner;
};
} // namespace

comboBox::comboBox(theme& colors, QWidget* parent) : QComboBox(parent)
{
    comboStyle* selectStyle = new comboStyle;
    selectStyle->setParent(this);
    setStyle(selectStyle);
    setAttribute(Qt::WA_Hover);
    setMinimumHeight(uiSize::controlHeight);
    setMinimumWidth(uiSize::selectWidth);
    setCursor(Qt::PointingHandCursor);
    setMaxVisibleItems(8);
    QListView* list = new QListView(this);
    list->setObjectName("comboList");
    list->setMouseTracking(true);
    list->setUniformItemSizes(true);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setItemDelegate(new comboItem(this));
    setView(list);
    QWidget* popup = view()->window();
    popup->setObjectName("comboPopup");
    popup->installEventFilter(this);
    if (QFrame* frame = qobject_cast<QFrame*>(popup))
    {
        frame->setFrameStyle(QFrame::NoFrame);
    }
    popup->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    popup->setAttribute(Qt::WA_TranslucentBackground);
    if (popup->layout())
    {
        popup->layout()->setContentsMargins(uiSize::shadowPad, uiSize::shadowPad, uiSize::shadowPad,
                                            uiSize::shadowPad);
    }
    connect(&colors, &theme::changed, this,
            [this]
            {
                update();
                view()->viewport()->update();
                view()->window()->update();
                if (expanded)
                {
                    movePopup();
                }
            });
}

QSize comboBox::sizeHint() const
{
    QSize size = QComboBox::sizeHint();
    size.setHeight(qMax(uiSize::controlHeight, fontMetrics().height() + uiSize::space16));
    return size;
}

QRect comboBox::placePopup(const QRect& anchor, const QRect& area, const QSize& size)
{
    const int below = qMax(0, area.bottom() - anchor.bottom() - uiSize::popupGap);
    const int above = qMax(0, anchor.top() - area.top() - uiSize::popupGap);
    const bool down = below >= size.height() || (above < size.height() && below >= above);
    const int height = qMin(size.height(), down ? below : above);
    const int width = qMin(size.width(), area.width());
    const int left = qBound(area.left(), anchor.left(), area.right() - width + 1);
    const int top = down ? anchor.bottom() + 1 + uiSize::popupGap : anchor.top() - uiSize::popupGap - height;
    return QRect(left, top, width, height);
}

void comboBox::movePopup()
{
    const QRect anchor(mapToGlobal(QPoint(0, 0)), size());
    QScreen* target = QGuiApplication::screenAt(anchor.center());
    if (!target)
    {
        target = screen();
    }
    const QRect area = target->availableGeometry().adjusted(uiSize::shadowPad, uiSize::shadowPad,
                                                            -uiSize::shadowPad, -uiSize::shadowPad);
    int rows = qMin(count(), maxVisibleItems());
    int rowHeight = view()->sizeHintForRow(0);
    if (rowHeight <= 0)
    {
        rowHeight = uiSize::optionHeight;
    }
    const QSize desired(width(), rows * rowHeight + uiSize::space12);
    const QRect surface = placePopup(anchor, area, desired);
    QWidget* popup = view()->window();
    popup->setMinimumSize(0, 0);
    popup->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    popup->setGeometry(
        surface.adjusted(-uiSize::shadowPad, -uiSize::shadowPad, uiSize::shadowPad, uiSize::shadowPad));
    if (popup->layout())
    {
        popup->layout()->activate();
    }
    view()->scrollTo(view()->currentIndex(), QAbstractItemView::EnsureVisible);
}

void comboBox::showPopup()
{
    if (count() == 0 || !isEnabled())
    {
        return;
    }
    const bool animate = qApp->isEffectEnabled(Qt::UI_AnimateCombo);
    qApp->setEffectEnabled(Qt::UI_AnimateCombo, false);
    QComboBox::showPopup();
    qApp->setEffectEnabled(Qt::UI_AnimateCombo, animate);
    // A cancelled keyboard preview must not become the next popup's starting row.
    view()->setCurrentIndex(model()->index(currentIndex(), modelColumn(), rootModelIndex()));
    if (QFrame* frame = qobject_cast<QFrame*>(view()->window()))
    {
        frame->setFrameStyle(QFrame::NoFrame);
    }
    expanded = true;
    movePopup();
    update();
}

void comboBox::hidePopup()
{
    expanded = false;
    QComboBox::hidePopup();
    update();
}

bool comboBox::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == view()->window() && event->type() == QEvent::Paint)
    {
        QWidget* popup = view()->window();
        QPainter painter(popup);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(Qt::NoBrush);
        const QRectF surface =
            QRectF(popup->rect())
                .adjusted(uiSize::shadowPad, uiSize::shadowPad, -uiSize::shadowPad, -uiSize::shadowPad);
        // Paint within the popup bounds; graphics effects can expand layered-window dirty regions.
        for (int spread = 6; spread > 0; --spread)
        {
            QColor shade = theme::tokens().shadow;
            shade.setAlpha(shade.alpha() * (7 - spread) / 18);
            painter.setPen(QPen(shade, 1));
            painter.drawRoundedRect(surface.adjusted(-spread, -spread + 1, spread, spread + 1),
                                    uiSize::controlRadius + spread, uiSize::controlRadius + spread);
        }
        return true;
    }
    if (watched == view()->window() && event->type() == QEvent::Hide)
    {
        // Qt also hides the popup internally on Escape and outside clicks.
        expanded = false;
        update();
    }
    return QComboBox::eventFilter(watched, event);
}

void comboBox::changeEvent(QEvent* event)
{
    QComboBox::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange ||
        event->type() == QEvent::FontChange)
    {
        update();
        view()->viewport()->update();
    }
}

void comboBox::paintEvent(QPaintEvent*)
{
    const themeColors& color = theme::tokens();
    QStyleOptionComboBox option;
    initStyleOption(&option);
    const bool pressed = expanded || option.state.testFlag(QStyle::State_Sunken);
    QColor fill =
        !isEnabled() ? color.hover : (pressed ? color.pressed : (underMouse() ? color.hover : color.surface));
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(hasFocus() ? color.focus : color.border, 1));
    painter.setBrush(fill);
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), uiSize::controlRadius,
                            uiSize::controlRadius);
    painter.setPen(isEnabled() ? color.text : color.disabled);
    QRect textArea = rect().adjusted(uiSize::space12, 0, -uiSize::space24 - uiSize::space8, 0);
    painter.drawText(textArea, Qt::AlignLeft | Qt::AlignVCenter,
                     fontMetrics().elidedText(currentText(), Qt::ElideRight, textArea.width()));
    QPointF center(width() - uiSize::space16, height() / 2.0);
    QPainterPath arrow;
    const qreal offset = expanded ? -1 : 1;
    arrow.moveTo(center + QPointF(-4, -2 * offset));
    arrow.lineTo(center + QPointF(0, 2 * offset));
    arrow.lineTo(center + QPointF(4, -2 * offset));
    painter.setPen(
        QPen(isEnabled() ? color.muted : color.disabled, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(arrow);
}
