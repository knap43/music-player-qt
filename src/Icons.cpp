#include "Icons.h"

#include "Theme.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace Icons {

namespace {

QPolygonF poly(std::initializer_list<QPointF> points)
{
    return QPolygonF(QVector<QPointF>(points));
}

void drawGlyph(QPainter &p, Kind kind, const QColor &color)
{
    // Coordinates are in a 24x24 grid.
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    QPen stroke(color, 2.0, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);

    switch (kind) {
    case Kind::Play:
        p.drawPolygon(poly({{7, 4.5}, {19.5, 12}, {7, 19.5}}));
        break;
    case Kind::Pause:
        p.drawRect(QRectF(6, 5, 4, 14));
        p.drawRect(QRectF(14, 5, 4, 14));
        break;
    case Kind::Next:
        p.drawPolygon(poly({{5, 5}, {15.5, 12}, {5, 19}}));
        p.drawRect(QRectF(16, 5, 3, 14));
        break;
    case Kind::Previous:
        p.drawRect(QRectF(5, 5, 3, 14));
        p.drawPolygon(poly({{19, 5}, {8.5, 12}, {19, 19}}));
        break;
    case Kind::Shuffle: {
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke);
        p.drawPolyline(poly({{3, 7}, {8, 7}, {14, 17}, {18, 17}}));
        p.drawPolyline(poly({{3, 17}, {8, 17}, {14, 7}, {18, 7}}));
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPolygon(poly({{17.5, 3.5}, {22, 7}, {17.5, 10.5}}));
        p.drawPolygon(poly({{17.5, 13.5}, {22, 17}, {17.5, 20.5}}));
        break;
    }
    case Kind::Repeat:
    case Kind::RepeatOne: {
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke);
        p.drawPolyline(poly({{4, 13}, {4, 7}, {17, 7}}));
        p.drawPolyline(poly({{20, 11}, {20, 17}, {7, 17}}));
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPolygon(poly({{16.5, 3.5}, {21, 7}, {16.5, 10.5}}));
        p.drawPolygon(poly({{7.5, 13.5}, {3, 17}, {7.5, 20.5}}));
        if (kind == Kind::RepeatOne) {
            p.drawRect(QRectF(11.5, 9, 2, 6));
            p.drawRect(QRectF(10, 9, 2, 1.6));
        }
        break;
    }
    case Kind::Volume:
    case Kind::Muted: {
        p.drawPolygon(poly({{3, 9}, {7, 9}, {12, 4.5}, {12, 19.5}, {7, 15}, {3, 15}}));
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke);
        if (kind == Kind::Volume) {
            p.drawArc(QRectF(8, 8, 8, 8), -50 * 16, 100 * 16);
            p.drawArc(QRectF(5, 4, 16, 16), -50 * 16, 100 * 16);
        } else {
            p.drawLine(QPointF(15.5, 9), QPointF(21, 14.5));
            p.drawLine(QPointF(21, 9), QPointF(15.5, 14.5));
        }
        break;
    }
    case Kind::Note: {
        p.save();
        p.translate(8, 17.5);
        p.rotate(-20);
        p.drawEllipse(QPointF(0, 0), 3.8, 2.8);
        p.restore();
        p.drawRect(QRectF(10.6, 3.5, 1.8, 14));
        p.drawPolygon(poly({{12.4, 3.5}, {18.5, 6.5}, {18.5, 10}, {12.4, 7.2}}));
        break;
    }
    case Kind::Clear: {
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke);
        p.drawLine(QPointF(7, 7), QPointF(17, 17));
        p.drawLine(QPointF(17, 7), QPointF(7, 17));
        break;
    }
    case Kind::Search: {
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke);
        p.drawRect(QRectF(4, 4, 11, 11));
        p.drawLine(QPointF(15, 15), QPointF(20, 20));
        break;
    }
    }
}

QPixmap render(Kind kind, const QColor &color, int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 24.0, size / 24.0);
    drawGlyph(p, kind, color);
    return pm;
}

} // namespace

void paint(QPainter &painter, Kind kind, const QRectF &rect, const QColor &color)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(rect.topLeft());
    painter.scale(rect.width() / 24.0, rect.height() / 24.0);
    drawGlyph(painter, kind, color);
    painter.restore();
}

QIcon icon(Kind kind, const QColor &normal, const QColor &on)
{
    QIcon result;
    QColor hover = normal.lightness() > 128 ? QColor(Qt::white) : normal;
    QColor disabled = Theme::textFaint;
    for (int size : {16, 20, 24, 32, 40, 48, 64}) {
        result.addPixmap(render(kind, normal, size), QIcon::Normal, QIcon::Off);
        result.addPixmap(render(kind, hover, size), QIcon::Active, QIcon::Off);
        result.addPixmap(render(kind, disabled, size), QIcon::Disabled, QIcon::Off);
        if (on.isValid()) {
            result.addPixmap(render(kind, on, size), QIcon::Normal, QIcon::On);
            result.addPixmap(render(kind, on, size), QIcon::Active, QIcon::On);
            result.addPixmap(render(kind, disabled, size), QIcon::Disabled, QIcon::On);
        }
    }
    return result;
}

QIcon appIcon()
{
    QIcon result;
    for (int size : {16, 24, 32, 48, 64, 128, 256}) {
        QPixmap pm(size, size);
        pm.fill(Theme::accent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal inset = size * 0.14;
        paint(p, Kind::Note, QRectF(inset, inset, size - 2 * inset, size - 2 * inset), Theme::onAccent);
        p.end();
        result.addPixmap(pm);
    }
    return result;
}

} // namespace Icons
