#pragma once

#include <QMouseEvent>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>

// A horizontal slider that jumps straight to the clicked position instead of
// stepping by a page, and can then be dragged as usual.
class JumpSlider : public QSlider
{
public:
    explicit JumpSlider(QWidget *parent = nullptr)
        : QSlider(Qt::Horizontal, parent)
    {
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            QStyleOptionSlider opt;
            initStyleOption(&opt);
            const QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt,
                                                         QStyle::SC_SliderHandle, this);
            if (!handle.contains(event->position().toPoint())) {
                const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt,
                                                             QStyle::SC_SliderGroove, this);
                const int span = groove.width() - handle.width();
                const int x = int(event->position().x()) - groove.x() - handle.width() / 2;
                const int value = QStyle::sliderValueFromPosition(minimum(), maximum(), x, span,
                                                                  opt.upsideDown);
                setSliderDown(true);
                setValue(value);
                triggerAction(SliderMove);
            }
        }
        // Continue with normal handling so the handle is grabbed for dragging.
        QSlider::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        QSlider::mouseReleaseEvent(event);
        if (isSliderDown())
            setSliderDown(false);
    }
};
