#include "PlayerController.h"

#include "MetadataReader.h"
#include "PlayQueue.h"

#include <QAudioOutput>
#include <QFileInfo>
#include <QMediaMetaData>
#include <QTimer>
#include <QUrl>

#include <cmath>

PlayerController::PlayerController(PlayQueue *queue, QObject *parent)
    : QObject(parent)
    , m_queue(queue)
    , m_player(new QMediaPlayer(this))
    , m_audio(new QAudioOutput(this))
    , m_ticker(new QTimer(this))
{
    m_player->setAudioOutput(m_audio);
    setVolume(m_volume);

    // QMediaPlayer reports progress fairly coarsely on some backends; poll
    // while playing so the seek bar and synced lyrics move smoothly.
    m_ticker->setInterval(50);
    connect(m_ticker, &QTimer::timeout, this, [this] { emit positionChanged(position()); });

    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        const bool playing = state == QMediaPlayer::PlayingState;
        if (playing)
            m_ticker->start();
        else
            m_ticker->stop();
        emit playingChanged(playing);
        emit positionChanged(position());
    });
    connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64) {
        if (!m_ticker->isActive())
            emit positionChanged(position());
    });
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &PlayerController::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::metaDataChanged, this, &PlayerController::onMetaDataChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &PlayerController::onDurationChanged);
    connect(m_player, &QMediaPlayer::errorOccurred, this, &PlayerController::onError);
    connect(m_audio, &QAudioOutput::mutedChanged, this, &PlayerController::mutedChanged);
}

bool PlayerController::isPlaying() const
{
    return m_player->playbackState() == QMediaPlayer::PlayingState;
}

qint64 PlayerController::position() const
{
    return m_pendingSeek >= 0 ? m_pendingSeek : m_player->position();
}

qint64 PlayerController::duration() const
{
    const qint64 d = m_player->duration();
    return d > 0 ? d : m_track.durationMs;
}

bool PlayerController::isMuted() const
{
    return m_audio->isMuted();
}

void PlayerController::load(int row, bool autoplay)
{
    if (row < 0 || row >= m_queue->count())
        return;

    m_track = m_queue->trackAt(row);
    m_reachedEnd = false;
    m_pendingSeek = -1;

    const QUrl url = QUrl::fromLocalFile(m_track.path);
    if (m_player->source() == url)
        m_player->setPosition(0);
    else
        m_player->setSource(url);

    const QImage cover = Metadata::readCover(m_track.path);
    m_hasCover = !cover.isNull();

    emit trackChanged(m_track);
    emit coverChanged(cover);
    emit lyricsChanged(Metadata::readLyrics(m_track.path));
    emit durationChanged(m_track.durationMs);
    emit positionChanged(0);

    if (autoplay)
        m_player->play();
}

void PlayerController::playRow(int row)
{
    if (row < 0 || row >= m_queue->count())
        return;
    m_queue->setCurrentRow(row);
    load(row, true);
}

void PlayerController::togglePlayPause()
{
    if (isPlaying())
        pause();
    else
        play();
}

void PlayerController::play()
{
    if (m_track.isValid() && !m_reachedEnd) {
        m_player->play();
        if (m_pendingSeek >= 0) {
            m_player->setPosition(m_pendingSeek);
            m_pendingSeek = -1;
        }
        return;
    }

    // Nothing loaded yet, or the queue ran out: continue with whatever comes
    // next, and start over from the top if there is nothing left.
    int row = -1;
    if (!m_track.isValid() && m_queue->currentRow() >= 0)
        row = m_queue->currentRow();
    else
        row = m_queue->advance(false);
    if (row < 0 && m_queue->count() > 0) {
        row = 0;
        m_queue->setCurrentRow(row);
    }
    if (row >= 0)
        load(row, true);
}

void PlayerController::pause()
{
    m_player->pause();
}

void PlayerController::stop()
{
    m_player->stop();
}

void PlayerController::next()
{
    const int row = m_queue->advance(false);
    if (row >= 0) {
        load(row, true);
    } else {
        m_player->stop();
        m_reachedEnd = true;
    }
}

void PlayerController::previous()
{
    if (position() > 3000) {
        seek(0);
        return;
    }
    const int row = m_queue->goBack();
    if (row >= 0)
        load(row, true);
    else
        seek(0);
}

void PlayerController::seek(qint64 positionMs)
{
    if (!m_track.isValid())
        return;
    if (m_reachedEnd) {
        // Seeking after the queue finished replays the last track from there.
        m_reachedEnd = false;
        m_player->play();
    }
    if (m_pendingSeek >= 0 && !isPlaying())
        m_pendingSeek = positionMs;
    else
        m_player->setPosition(positionMs);
    emit positionChanged(positionMs);
}

void PlayerController::setVolume(int percent)
{
    m_volume = qBound(0, percent, 100);
    // Cubic curve: slider positions map to roughly even loudness steps.
    const double v = m_volume / 100.0;
    m_audio->setVolume(float(v * v * v));
    emit volumeChanged(m_volume);
}

void PlayerController::setMuted(bool muted)
{
    m_audio->setMuted(muted);
}

void PlayerController::restoreCurrent(qint64 positionMs)
{
    const int row = m_queue->currentRow();
    if (row < 0)
        return;
    load(row, false);
    if (positionMs > 0) {
        m_pendingSeek = positionMs;
        emit positionChanged(positionMs);
    }
}

void PlayerController::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    switch (status) {
    case QMediaPlayer::BufferedMedia:
        m_consecutiveErrors = 0;
        break;
    case QMediaPlayer::EndOfMedia: {
        const int row = m_queue->advance(true);
        if (row >= 0) {
            load(row, true);
        } else {
            m_reachedEnd = true;
            emit positionChanged(duration());
        }
        break;
    }
    default:
        break;
    }
}

void PlayerController::updateQueueEntry()
{
    const int row = m_queue->currentRow();
    if (row >= 0 && m_queue->trackAt(row).path == m_track.path)
        m_queue->updateTrack(row, m_track);
}

void PlayerController::onMetaDataChanged()
{
    const QMediaMetaData md = m_player->metaData();

    // Fall back to the stream's own artwork (e.g. WebM with an older TagLib).
    if (!m_hasCover) {
        QImage image = md.value(QMediaMetaData::CoverArtImage).value<QImage>();
        if (image.isNull())
            image = md.value(QMediaMetaData::ThumbnailImage).value<QImage>();
        if (!image.isNull()) {
            m_hasCover = true;
            emit coverChanged(image);
        }
    }

    if (!m_track.tagged) {
        const QString title = md.stringValue(QMediaMetaData::Title).trimmed();
        const QString artist = md.stringValue(QMediaMetaData::ContributingArtist).trimmed();
        const QString albumArtist = md.stringValue(QMediaMetaData::AlbumArtist).trimmed();
        if (title.isEmpty() && artist.isEmpty() && albumArtist.isEmpty())
            return;
        if (!title.isEmpty())
            m_track.title = title;
        if (!artist.isEmpty())
            m_track.artist = artist;
        else if (!albumArtist.isEmpty())
            m_track.artist = albumArtist;
        m_track.tagged = true;
        updateQueueEntry();
        emit trackChanged(m_track);
    }
}

void PlayerController::onDurationChanged(qint64 duration)
{
    if (duration <= 0)
        return;
    emit durationChanged(duration);
    if (qAbs(m_track.durationMs - duration) > 1000) {
        m_track.durationMs = duration;
        updateQueueEntry();
    }
}

void PlayerController::onError(QMediaPlayer::Error error, const QString &message)
{
    if (error == QMediaPlayer::NoError)
        return;
    const QString name = QFileInfo(m_track.path).fileName();
    emit errorOccurred(tr("Cannot play \"%1\": %2").arg(name, message));

    // Skip unplayable files, but give up once every entry has failed.
    if (++m_consecutiveErrors >= m_queue->count()) {
        m_consecutiveErrors = 0;
        m_player->stop();
        return;
    }
    QTimer::singleShot(0, this, [this] {
        const int row = m_queue->advance(false);
        if (row >= 0)
            load(row, true);
    });
}
