#include "CoverLyricsView.h"

#include "Icons.h"
#include "ImageUtils.h"
#include "Theme.h"

#include <QEasingCurve>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QWheelEvent>

namespace {

constexpr int kSideMargin = 32;
constexpr qreal kFocusLine = 0.42; // current lyric sits at 42% of the height
constexpr qreal kPlainTopPadding = 56;
constexpr int kCoverMargin = 32;
const QString kInstrumental = QStringLiteral("♪");

} // namespace

CoverLyricsView::CoverLyricsView(QWidget *parent)
    : QWidget(parent)
    , m_resumeTimer(new QTimer(this))
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_scrollAnimation.setDuration(420);
    m_scrollAnimation.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_scrollAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_scroll = v.toReal();
        update();
    });

    m_resumeTimer->setSingleShot(true);
    m_resumeTimer->setInterval(3500);
    connect(m_resumeTimer, &QTimer::timeout, this, &CoverLyricsView::resumeFollowing);
}

void CoverLyricsView::setCover(const QImage &cover)
{
    m_cover = cover;
    m_blurred = ImageUtils::blurred(cover);
    m_coverScaled = QPixmap();
    m_blurScaled = QPixmap();
    update();
}

void CoverLyricsView::setLyrics(const Lyrics &lyrics)
{
    m_lyrics = lyrics;
    m_currentLine = -1;
    m_hoverLine = -1;
    m_userScrolling = false;
    m_resumeTimer->stop();
    m_scrollAnimation.stop();
    m_layoutWidth = -1;
    relayout();
    m_scroll = m_lyrics.isSynced() ? targetScrollFor(0) : clampScroll(-1e9);
    setCursor(Qt::ArrowCursor);
    update();
}

void CoverLyricsView::setShowLyrics(bool show)
{
    if (m_showLyrics == show)
        return;
    m_showLyrics = show;
    if (m_lyrics.isSynced())
        m_scroll = targetScrollFor(qMax(0, m_currentLine));
    update();
}

void CoverLyricsView::setHasTrack(bool hasTrack)
{
    m_hasTrack = hasTrack;
    update();
}

void CoverLyricsView::setPosition(qint64 positionMs)
{
    if (!m_lyrics.isSynced())
        return;
    const int line = m_lyrics.lineAt(positionMs);
    if (line == m_currentLine)
        return;
    m_currentLine = line;
    if (!m_userScrolling)
        scrollTo(targetScrollFor(qMax(0, line)), lyricsVisible() && isVisible());
    update();
}

QRectF CoverLyricsView::lyricsRect() const
{
    return QRectF(rect()).adjusted(kSideMargin, 0, -kSideMargin, 0);
}

QFont CoverLyricsView::lyricsFont() const
{
    QFont f = font();
    f.setPixelSize(qBound(15, width() / 27, 26));
    f.setWeight(QFont::DemiBold);
    return f;
}

void CoverLyricsView::relayout()
{
    m_layout.clear();
    const QRectF area = lyricsRect();
    m_layoutWidth = area.width();
    if (m_lyrics.isEmpty() || area.width() <= 0)
        return;

    const QFontMetricsF fm(lyricsFont());
    const qreal spacing = fm.height() * 0.6;
    qreal y = 0;
    for (const LyricLine &line : m_lyrics.lines()) {
        const QString text = line.text.trimmed();
        qreal height;
        if (text.isEmpty() && !m_lyrics.isSynced()) {
            height = fm.height() * 0.4; // paragraph break
        } else {
            const QString shown = text.isEmpty() ? kInstrumental : text;
            height = fm.boundingRect(QRectF(0, 0, area.width(), 1e6),
                                     Qt::AlignHCenter | Qt::TextWordWrap, shown)
                         .height();
        }
        m_layout.push_back({y, height});
        y += height + spacing;
    }
}

qreal CoverLyricsView::contentHeight() const
{
    if (m_layout.isEmpty())
        return 0;
    return m_layout.last().top + m_layout.last().height;
}

qreal CoverLyricsView::targetScrollFor(int line) const
{
    if (m_layout.isEmpty())
        return 0;
    line = qBound(0, line, int(m_layout.size()) - 1);
    const LineLayout &l = m_layout[line];
    return l.top + l.height / 2 - height() * kFocusLine;
}

qreal CoverLyricsView::clampScroll(qreal value) const
{
    if (m_layout.isEmpty())
        return 0;
    if (m_lyrics.isSynced())
        return qBound(targetScrollFor(0), value, targetScrollFor(int(m_layout.size()) - 1));
    // Short lyrics are centred vertically; long ones start near the top.
    const qreal min = qMin(-kPlainTopPadding, -(height() - contentHeight()) / 2);
    const qreal max = qMax(min, contentHeight() - height() + kPlainTopPadding);
    return qBound(min, value, max);
}

void CoverLyricsView::scrollTo(qreal target, bool animated)
{
    m_scrollAnimation.stop();
    if (!animated || qAbs(target - m_scroll) < 0.5) {
        m_scroll = target;
        update();
        return;
    }
    m_scrollAnimation.setStartValue(m_scroll);
    m_scrollAnimation.setEndValue(target);
    m_scrollAnimation.start();
}

void CoverLyricsView::resumeFollowing()
{
    m_userScrolling = false;
    if (m_lyrics.isSynced())
        scrollTo(targetScrollFor(qMax(0, m_currentLine)), true);
}

int CoverLyricsView::lineAtPoint(const QPointF &point) const
{
    if (!lyricsVisible() || !lyricsRect().contains(point))
        return -1;
    const qreal y = point.y() + m_scroll;
    for (int i = 0; i < m_layout.size(); ++i) {
        if (y >= m_layout[i].top - 4 && y <= m_layout[i].top + m_layout[i].height + 4)
            return i;
    }
    return -1;
}

void CoverLyricsView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_coverScaled = QPixmap();
    m_blurScaled = QPixmap();
    relayout();
    if (m_lyrics.isSynced() && !m_userScrolling) {
        m_scrollAnimation.stop();
        m_scroll = targetScrollFor(qMax(0, m_currentLine));
    } else {
        m_scroll = clampScroll(m_scroll);
    }
}

void CoverLyricsView::wheelEvent(QWheelEvent *event)
{
    if (!lyricsVisible()) {
        event->ignore();
        return;
    }
    qreal delta = event->pixelDelta().y();
    if (delta == 0)
        delta = event->angleDelta().y() / 120.0 * 64.0;
    m_scrollAnimation.stop();
    m_scroll = clampScroll(m_scroll - delta);
    if (m_lyrics.isSynced()) {
        m_userScrolling = true;
        m_resumeTimer->start();
    }
    update();
    event->accept();
}

void CoverLyricsView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_lyrics.isSynced()) {
        const int line = lineAtPoint(event->position());
        if (line >= 0) {
            m_userScrolling = false;
            m_resumeTimer->stop();
            emit seekRequested(m_lyrics.lines()[line].timeMs);
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void CoverLyricsView::mouseMoveEvent(QMouseEvent *event)
{
    const int line = m_lyrics.isSynced() ? lineAtPoint(event->position()) : -1;
    if (line != m_hoverLine) {
        m_hoverLine = line;
        setCursor(line >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void CoverLyricsView::leaveEvent(QEvent *event)
{
    if (m_hoverLine != -1) {
        m_hoverLine = -1;
        setCursor(Qt::ArrowCursor);
        update();
    }
    QWidget::leaveEvent(event);
}

void CoverLyricsView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    if (lyricsVisible()) {
        paintBackdrop(p, 0.62);
        paintLyrics(p);
    } else if (!m_cover.isNull()) {
        paintBackdrop(p, 0.6);
        paintCover(p);
    } else {
        paintPlaceholder(p);
    }
}

void CoverLyricsView::paintBackdrop(QPainter &p, qreal dim)
{
    if (m_blurred.isNull()) {
        QLinearGradient g(0, 0, 0, height());
        g.setColorAt(0, Theme::base2);
        g.setColorAt(1, Theme::base0);
        p.fillRect(rect(), g);
        return;
    }
    const qreal dpr = devicePixelRatioF();
    const QSize target = (QSizeF(size()) * dpr).toSize();
    if (m_blurScaled.isNull() || m_blurScaled.size() != target) {
        const QRect crop = ImageUtils::coverCrop(m_blurred.size(), size());
        // Trim the blurred edges, which bleed in from the clamped border.
        const QRect inner = crop.adjusted(crop.width() / 12, crop.height() / 12,
                                          -crop.width() / 12, -crop.height() / 12);
        m_blurScaled = QPixmap::fromImage(
            m_blurred.copy(inner).scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        m_blurScaled.setDevicePixelRatio(dpr);
    }
    p.drawPixmap(0, 0, m_blurScaled);
    p.fillRect(rect(), QColor(0, 0, 0, int(dim * 255)));
}

void CoverLyricsView::paintCover(QPainter &p)
{
    const int available = qMax(16, qMin(width(), height()) - 2 * kCoverMargin);
    const QSize fitted = m_cover.size().scaled(available, available, Qt::KeepAspectRatio);
    const QRect target(QPoint((width() - fitted.width()) / 2, (height() - fitted.height()) / 2),
                       fitted);

    // A soft, square drop shadow.
    for (int i = 1; i <= 14; ++i)
        p.fillRect(target.adjusted(-i, -i + 6, i, i + 6), QColor(0, 0, 0, 9));

    const qreal dpr = devicePixelRatioF();
    const QSize pixels = (QSizeF(fitted) * dpr).toSize();
    if (m_coverScaled.isNull() || m_coverScaled.size() != pixels) {
        m_coverScaled = QPixmap::fromImage(
            m_cover.scaled(pixels, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        m_coverScaled.setDevicePixelRatio(dpr);
    }
    p.drawPixmap(target.topLeft(), m_coverScaled);
}

void CoverLyricsView::paintPlaceholder(QPainter &p)
{
    p.fillRect(rect(), Theme::base0);
    const int side = qMax(16, qMin(width(), height()) - 2 * kCoverMargin);
    const QRect square(QPoint((width() - side) / 2, (height() - side) / 2), QSize(side, side));
    p.fillRect(square, Theme::base2);
    const qreal glyph = side * 0.36;
    Icons::paint(p, Icons::Kind::Note,
                 QRectF(square.center().x() - glyph / 2, square.center().y() - glyph / 2, glyph, glyph),
                 Theme::base4);
    if (!m_hasTrack) {
        QFont f = font();
        f.setPixelSize(qBound(12, side / 22, 16));
        p.setFont(f);
        p.setPen(Theme::textFaint);
        p.drawText(QRectF(square.left(), square.center().y() + glyph / 2 + 12, square.width(), 40),
                   Qt::AlignHCenter | Qt::AlignTop, tr("Nothing playing"));
    }
}

void CoverLyricsView::paintLyrics(QPainter &p)
{
    if (qAbs(m_layoutWidth - lyricsRect().width()) > 0.5)
        relayout();

    const qreal dpr = devicePixelRatioF();
    QImage layer((QSizeF(size()) * dpr).toSize(), QImage::Format_ARGB32_Premultiplied);
    layer.setDevicePixelRatio(dpr);
    layer.fill(Qt::transparent);

    {
        QPainter lp(&layer);
        lp.setRenderHint(QPainter::TextAntialiasing);
        lp.setFont(lyricsFont());
        const QRectF area = lyricsRect();
        const bool synced = m_lyrics.isSynced();
        const QVector<LyricLine> &lines = m_lyrics.lines();

        for (int i = 0; i < m_layout.size(); ++i) {
            const qreal top = area.top() + m_layout[i].top - m_scroll;
            if (top > height() || top + m_layout[i].height < 0)
                continue;
            QString text = lines[i].text.trimmed();
            if (text.isEmpty()) {
                if (!synced)
                    continue;
                text = kInstrumental;
            }

            QColor color(Qt::white);
            if (synced) {
                if (i == m_currentLine)
                    color = Theme::accent;
                else if (i == m_hoverLine)
                    color.setAlpha(235);
                else if (i < m_currentLine)
                    color.setAlpha(95);
                else
                    color.setAlpha(150);
            } else {
                color.setAlpha(225);
            }
            lp.setPen(color);
            lp.drawText(QRectF(area.left(), top, area.width(), m_layout[i].height),
                        Qt::AlignHCenter | Qt::TextWordWrap, text);
        }

        // Fade the lyrics out towards the top and bottom edges.
        QLinearGradient fade(0, 0, 0, height());
        fade.setColorAt(0.0, QColor(0, 0, 0, 0));
        fade.setColorAt(0.13, QColor(0, 0, 0, 255));
        fade.setColorAt(0.87, QColor(0, 0, 0, 255));
        fade.setColorAt(1.0, QColor(0, 0, 0, 0));
        lp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        lp.fillRect(QRectF(0, 0, width(), height()), fade);
    }

    p.drawImage(0, 0, layer);
}
