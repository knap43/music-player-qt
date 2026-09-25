#include "ImageUtils.h"

#include <QVector>

namespace {

// One horizontal box-blur pass over premultiplied ARGB, with clamped edges.
void blurRows(const QImage &src, QImage &dst, int r)
{
    const int w = src.width();
    const int h = src.height();
    const int window = 2 * r + 1;
    for (int y = 0; y < h; ++y) {
        const QRgb *in = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        QRgb *out = reinterpret_cast<QRgb *>(dst.scanLine(y));
        int a = 0, rr = 0, g = 0, b = 0;
        for (int i = -r; i <= r; ++i) {
            const QRgb p = in[qBound(0, i, w - 1)];
            a += qAlpha(p);
            rr += qRed(p);
            g += qGreen(p);
            b += qBlue(p);
        }
        for (int x = 0; x < w; ++x) {
            out[x] = qRgba(rr / window, g / window, b / window, a / window);
            const QRgb add = in[qMin(x + r + 1, w - 1)];
            const QRgb sub = in[qMax(x - r, 0)];
            a += qAlpha(add) - qAlpha(sub);
            rr += qRed(add) - qRed(sub);
            g += qGreen(add) - qGreen(sub);
            b += qBlue(add) - qBlue(sub);
        }
    }
}

void blurColumns(const QImage &src, QImage &dst, int r)
{
    const int w = src.width();
    const int h = src.height();
    const int window = 2 * r + 1;
    QVector<const QRgb *> in(h);
    QVector<QRgb *> out(h);
    for (int y = 0; y < h; ++y) {
        in[y] = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        out[y] = reinterpret_cast<QRgb *>(dst.scanLine(y));
    }
    for (int x = 0; x < w; ++x) {
        int a = 0, rr = 0, g = 0, b = 0;
        for (int i = -r; i <= r; ++i) {
            const QRgb p = in[qBound(0, i, h - 1)][x];
            a += qAlpha(p);
            rr += qRed(p);
            g += qGreen(p);
            b += qBlue(p);
        }
        for (int y = 0; y < h; ++y) {
            out[y][x] = qRgba(rr / window, g / window, b / window, a / window);
            const QRgb add = in[qMin(y + r + 1, h - 1)][x];
            const QRgb sub = in[qMax(y - r, 0)][x];
            a += qAlpha(add) - qAlpha(sub);
            rr += qRed(add) - qRed(sub);
            g += qGreen(add) - qGreen(sub);
            b += qBlue(add) - qBlue(sub);
        }
    }
}

} // namespace

namespace ImageUtils {

QImage blurred(const QImage &source, int workSize, int radius, int passes)
{
    if (source.isNull())
        return {};
    QImage a = source.scaled(workSize, workSize, Qt::KeepAspectRatioByExpanding,
                             Qt::SmoothTransformation)
                   .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage b(a.size(), a.format());
    for (int i = 0; i < passes; ++i) {
        blurRows(a, b, radius);
        blurColumns(b, a, radius);
    }
    return a;
}

QRect coverCrop(const QSize &imageSize, const QSize &target)
{
    if (imageSize.isEmpty() || target.isEmpty())
        return QRect(QPoint(0, 0), imageSize);
    const QSize scaled = target.scaled(imageSize, Qt::KeepAspectRatio);
    return QRect(QPoint((imageSize.width() - scaled.width()) / 2,
                        (imageSize.height() - scaled.height()) / 2),
                 scaled);
}

QImage squareThumbnail(const QImage &source, int size)
{
    if (source.isNull())
        return {};
    const QRect crop = coverCrop(source.size(), QSize(1, 1));
    return source.copy(crop).scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

} // namespace ImageUtils
