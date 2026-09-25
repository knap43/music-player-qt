#pragma once

#include "Lyrics.h"
#include "Track.h"

#include <QImage>
#include <QMediaPlayer>
#include <QObject>

class PlayQueue;
class QAudioOutput;
class QTimer;

// Drives QMediaPlayer from the play queue and gathers everything the
// now-playing panel shows (artwork, lyrics, progress).
class PlayerController : public QObject
{
    Q_OBJECT
public:
    explicit PlayerController(PlayQueue *queue, QObject *parent = nullptr);

    PlayQueue *queue() const { return m_queue; }
    bool isPlaying() const;
    bool hasTrack() const { return m_track.isValid(); }
    const Track &currentTrack() const { return m_track; }
    qint64 position() const;
    qint64 duration() const;

    int volume() const { return m_volume; }
    bool isMuted() const;

public slots:
    void playRow(int row);
    void togglePlayPause();
    void play();
    void pause();
    void stop();
    void next();
    void previous();
    void seek(qint64 positionMs);
    void setVolume(int percent);
    void setMuted(bool muted);
    // Loads the queue's current row without starting playback, optionally
    // resuming at a saved position (used to restore the previous session).
    void restoreCurrent(qint64 positionMs);

signals:
    void trackChanged(const Track &track);
    void coverChanged(const QImage &cover);
    void lyricsChanged(const Lyrics &lyrics);
    void playingChanged(bool playing);
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void volumeChanged(int percent);
    void mutedChanged(bool muted);
    void errorOccurred(const QString &message);

private:
    void load(int row, bool autoplay);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onMetaDataChanged();
    void onDurationChanged(qint64 duration);
    void onError(QMediaPlayer::Error error, const QString &message);
    void updateQueueEntry();

    PlayQueue *m_queue;
    QMediaPlayer *m_player;
    QAudioOutput *m_audio;
    QTimer *m_ticker;

    Track m_track;
    bool m_hasCover = false;
    bool m_reachedEnd = false;   // stopped because the queue ran out
    qint64 m_pendingSeek = -1;
    int m_consecutiveErrors = 0;
    int m_volume = 80;
};
