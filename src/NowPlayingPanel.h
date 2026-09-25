#pragma once

#include "Lyrics.h"
#include "PlayQueue.h"
#include "Track.h"

#include <QWidget>

class CoverLyricsView;
class JumpSlider;
class QLabel;
class QToolButton;

// Right-hand side of the window: artwork (or lyrics over blurred artwork),
// track information, the seek bar and the transport controls.
class NowPlayingPanel : public QWidget
{
    Q_OBJECT
public:
    explicit NowPlayingPanel(QWidget *parent = nullptr);

    bool lyricsEnabled() const { return m_lyricsEnabled; }

public slots:
    void setTrack(const Track &track);
    void clearTrack();
    void setCover(const QImage &cover);
    void setLyrics(const Lyrics &lyrics);
    void setLyricsEnabled(bool enabled);
    void setPlaying(bool playing);
    void setPosition(qint64 positionMs);
    void setDuration(qint64 durationMs);
    void setShuffle(bool on);
    void setRepeat(PlayQueue::Repeat mode);
    void setVolume(int percent);
    void setMuted(bool muted);

signals:
    void playPauseClicked();
    void nextClicked();
    void previousClicked();
    void shuffleToggled(bool on);
    void repeatClicked();
    void seekRequested(qint64 positionMs);
    void volumeChangeRequested(int percent);
    void muteToggled(bool muted);
    void lyricsToggled(bool enabled);

private:
    QToolButton *makeButton(const QString &tooltip, int iconSize);
    void updateTimeLabels(qint64 positionMs);
    void updateVolumeIcon();

    CoverLyricsView *m_art;
    QLabel *m_title;
    QLabel *m_subtitle;
    QLabel *m_elapsed;
    QLabel *m_total;
    JumpSlider *m_seek;
    QToolButton *m_shuffle;
    QToolButton *m_previous;
    QToolButton *m_play;
    QToolButton *m_next;
    QToolButton *m_repeat;
    QToolButton *m_lyrics;
    QToolButton *m_mute;
    JumpSlider *m_volume;

    qint64 m_duration = 0;
    bool m_lyricsEnabled = true;
    bool m_muted = false;
};
