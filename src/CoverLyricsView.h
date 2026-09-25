#pragma once

#include "Lyrics.h"

#include <QElapsedTimer>
#include <QImage>
#include <QPixmap>
#include <QVariantAnimation>
#include <QWidget>

class QTimer;

// The artwork well of the now-playing panel. Shows the album cover; when the
// track has lyrics (and they are enabled) the cover is blurred and darkened
// and the lyrics are drawn on top of it. Synchronised lyrics follow playback,
// highlight the current line and can be clicked to seek.
class CoverLyricsView : public QWidget
{
    Q_OBJECT
public:
    explicit CoverLyricsView(QWidget *parent = nullptr);

    void setCover(const QImage &cover);
    void setLyrics(const Lyrics &lyrics);
    void setShowLyrics(bool show);
    void setPosition(qint64 positionMs);
    void setHasTrack(bool hasTrack);

    bool hasLyrics() const { return !m_lyrics.isEmpty(); }

    QSize sizeHint() const override { return {420, 420}; }
    QSize minimumSizeHint() const override { return {220, 220}; }

signals:
    void seekRequested(qint64 positionMs);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct LineLayout {
        qreal top = 0;
        qreal height = 0;
    };

    bool lyricsVisible() const { return m_showLyrics && !m_lyrics.isEmpty(); }
    QRectF lyricsRect() const;
    QFont lyricsFont() const;
    void relayout();
    qreal contentHeight() const;
    qreal targetScrollFor(int line) const;
    qreal clampScroll(qreal value) const;
    void scrollTo(qreal target, bool animated);
    int lineAtPoint(const QPointF &point) const;
    void resumeFollowing();

    void paintBackdrop(QPainter &p, qreal dim);
    void paintCover(QPainter &p);
    void paintPlaceholder(QPainter &p);
    void paintLyrics(QPainter &p);

    QImage m_cover;
    QImage m_blurred;
    QPixmap m_coverScaled;   // cached for the current size
    QPixmap m_blurScaled;
    bool m_hasTrack = false;

    Lyrics m_lyrics;
    bool m_showLyrics = true;
    QVector<LineLayout> m_layout;
    qreal m_layoutWidth = -1;
    int m_currentLine = -1;
    int m_hoverLine = -1;
    qreal m_scroll = 0;       // content y shown at the top of lyricsRect()
    QVariantAnimation m_scrollAnimation;
    bool m_userScrolling = false;
    QTimer *m_resumeTimer;
};
