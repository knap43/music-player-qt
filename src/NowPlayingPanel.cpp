#include "NowPlayingPanel.h"

#include "CoverLyricsView.h"
#include "Icons.h"
#include "JumpSlider.h"
#include "Theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

// A label that elides instead of forcing the window to grow.
class ElidedLabel : public QLabel
{
public:
    using QLabel::QLabel;

    QSize minimumSizeHint() const override { return {0, QLabel::minimumSizeHint().height()}; }
    QSize sizeHint() const override { return {0, QLabel::sizeHint().height()}; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setPen(palette().color(foregroundRole()));
        p.setFont(font());
        const QRect r = contentsRect();
        p.drawText(r, int(alignment()) | Qt::AlignVCenter,
                   fontMetrics().elidedText(text(), Qt::ElideRight, r.width()));
    }
};

} // namespace

NowPlayingPanel::NowPlayingPanel(QWidget *parent)
    : QWidget(parent)
    , m_art(new CoverLyricsView(this))
    , m_title(new ElidedLabel(this))
    , m_subtitle(new ElidedLabel(this))
    , m_elapsed(new QLabel(QStringLiteral("0:00"), this))
    , m_total(new QLabel(QStringLiteral("--:--"), this))
    , m_seek(new JumpSlider(this))
    , m_volume(new JumpSlider(this))
{
    setObjectName(QStringLiteral("nowPlaying"));
    setAttribute(Qt::WA_StyledBackground);

    m_shuffle = makeButton(tr("Shuffle (Ctrl+S)"), 20);
    m_previous = makeButton(tr("Previous (Ctrl+Left)"), 22);
    m_play = makeButton(tr("Play / Pause (Space)"), 26);
    m_next = makeButton(tr("Next (Ctrl+Right)"), 22);
    m_repeat = makeButton(tr("Repeat: off (Ctrl+R)"), 20);
    m_mute = makeButton(tr("Mute (M)"), 18);
    m_lyrics = new QToolButton(this);

    m_play->setObjectName(QStringLiteral("playButton"));
    m_play->setFixedSize(56, 56);
    for (QToolButton *b : {m_shuffle, m_previous, m_next, m_repeat})
        b->setFixedSize(42, 42);
    m_mute->setFixedSize(34, 34);

    m_shuffle->setCheckable(true);
    m_repeat->setCheckable(true);
    m_shuffle->setIcon(Icons::icon(Icons::Kind::Shuffle, Theme::textDim, Theme::accent));
    m_previous->setIcon(Icons::icon(Icons::Kind::Previous, Theme::text));
    m_next->setIcon(Icons::icon(Icons::Kind::Next, Theme::text));
    setRepeat(PlayQueue::Repeat::Off);
    setPlaying(false);
    updateVolumeIcon();

    m_lyrics->setObjectName(QStringLiteral("textToggle"));
    m_lyrics->setText(tr("LYRICS"));
    m_lyrics->setToolTip(tr("Show lyrics over the cover (L)"));
    m_lyrics->setCheckable(true);
    m_lyrics->setChecked(true);
    m_lyrics->setEnabled(false);
    m_lyrics->setFocusPolicy(Qt::NoFocus);
    m_lyrics->setCursor(Qt::PointingHandCursor);

    m_title->setObjectName(QStringLiteral("trackTitle"));
    m_subtitle->setObjectName(QStringLiteral("trackSubtitle"));
    QFont titleFont = m_title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.45);
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    m_title->setMinimumHeight(QFontMetrics(titleFont).height() + 4);

    m_elapsed->setObjectName(QStringLiteral("timeLabel"));
    m_total->setObjectName(QStringLiteral("timeLabel"));
    const int timeWidth = m_elapsed->fontMetrics().horizontalAdvance(QStringLiteral("00:00:00")) + 4;
    m_elapsed->setFixedWidth(timeWidth);
    m_total->setFixedWidth(timeWidth);
    m_elapsed->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_total->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_seek->setRange(0, 0);
    m_seek->setFocusPolicy(Qt::NoFocus);
    m_seek->setEnabled(false);
    m_volume->setRange(0, 100);
    m_volume->setFixedWidth(110);
    m_volume->setFocusPolicy(Qt::NoFocus);
    m_volume->setToolTip(tr("Volume"));

    // Layout
    auto *info = new QWidget(this);
    auto *infoLayout = new QVBoxLayout(info);
    infoLayout->setContentsMargins(28, 18, 28, 20);
    infoLayout->setSpacing(4);
    infoLayout->addWidget(m_title);
    infoLayout->addWidget(m_subtitle);
    infoLayout->addSpacing(10);

    auto *seekRow = new QHBoxLayout;
    seekRow->setSpacing(10);
    seekRow->addWidget(m_elapsed);
    seekRow->addWidget(m_seek, 1);
    seekRow->addWidget(m_total);
    infoLayout->addLayout(seekRow);
    infoLayout->addSpacing(8);

    auto *transport = new QHBoxLayout;
    transport->setSpacing(10);
    transport->addWidget(m_shuffle);
    transport->addWidget(m_previous);
    transport->addWidget(m_play);
    transport->addWidget(m_next);
    transport->addWidget(m_repeat);

    auto *volumeRow = new QHBoxLayout;
    volumeRow->setSpacing(4);
    volumeRow->addWidget(m_mute);
    volumeRow->addWidget(m_volume);

    auto *controls = new QGridLayout;
    controls->setContentsMargins(0, 0, 0, 0);
    controls->addWidget(m_lyrics, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
    controls->addLayout(transport, 0, 1, Qt::AlignCenter);
    controls->addLayout(volumeRow, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
    controls->setColumnStretch(0, 1);
    controls->setColumnStretch(2, 1);
    infoLayout->addLayout(controls);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_art, 1);
    layout->addWidget(info);

    // Wiring
    connect(m_play, &QToolButton::clicked, this, &NowPlayingPanel::playPauseClicked);
    connect(m_next, &QToolButton::clicked, this, &NowPlayingPanel::nextClicked);
    connect(m_previous, &QToolButton::clicked, this, &NowPlayingPanel::previousClicked);
    connect(m_shuffle, &QToolButton::toggled, this, &NowPlayingPanel::shuffleToggled);
    connect(m_repeat, &QToolButton::clicked, this, &NowPlayingPanel::repeatClicked);
    connect(m_mute, &QToolButton::clicked, this, [this] { emit muteToggled(!m_muted); });
    connect(m_lyrics, &QToolButton::toggled, this, [this](bool on) {
        setLyricsEnabled(on);
        emit lyricsToggled(on);
    });
    connect(m_volume, &QSlider::valueChanged, this, &NowPlayingPanel::volumeChangeRequested);
    connect(m_seek, &QSlider::sliderMoved, this, [this](int value) { updateTimeLabels(value); });
    connect(m_seek, &QSlider::sliderReleased, this, [this] { emit seekRequested(m_seek->value()); });
    connect(m_art, &CoverLyricsView::seekRequested, this, &NowPlayingPanel::seekRequested);

    clearTrack();
}

QToolButton *NowPlayingPanel::makeButton(const QString &tooltip, int iconSize)
{
    auto *button = new QToolButton(this);
    button->setToolTip(tooltip);
    button->setIconSize(QSize(iconSize, iconSize));
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoRaise(true);
    return button;
}

void NowPlayingPanel::setTrack(const Track &track)
{
    m_title->setText(track.displayTitle());
    m_title->setToolTip(track.displayTitle());
    QString subtitle = track.displayArtist() + QStringLiteral("  ·  ") + track.displayAlbum();
    if (track.year > 0)
        subtitle += QStringLiteral("  ·  ") + QString::number(track.year);
    m_subtitle->setText(subtitle);
    m_subtitle->setToolTip(track.path);
    m_seek->setEnabled(true);
    m_art->setHasTrack(true);
}

void NowPlayingPanel::clearTrack()
{
    m_title->setText(tr("Nothing playing"));
    m_title->setToolTip(QString());
    m_subtitle->setText(tr("Add something to the queue to get started"));
    m_subtitle->setToolTip(QString());
    m_seek->setEnabled(false);
    m_art->setHasTrack(false);
    m_art->setCover(QImage());
    setLyrics(Lyrics());
    setDuration(0);
    setPosition(0);
}

void NowPlayingPanel::setCover(const QImage &cover)
{
    m_art->setCover(cover);
}

void NowPlayingPanel::setLyrics(const Lyrics &lyrics)
{
    m_art->setLyrics(lyrics);
    m_lyrics->setEnabled(!lyrics.isEmpty());
    m_lyrics->setToolTip(lyrics.isEmpty()
                             ? tr("This track has no lyrics")
                             : (lyrics.isSynced() ? tr("Show synced lyrics over the cover (L)")
                                                  : tr("Show lyrics over the cover (L)")));
}

void NowPlayingPanel::setLyricsEnabled(bool enabled)
{
    m_lyricsEnabled = enabled;
    m_art->setShowLyrics(enabled);
    if (m_lyrics->isChecked() != enabled) {
        const QSignalBlocker blocker(m_lyrics);
        m_lyrics->setChecked(enabled);
    }
}

void NowPlayingPanel::setPlaying(bool playing)
{
    m_play->setIcon(Icons::icon(playing ? Icons::Kind::Pause : Icons::Kind::Play, Theme::onAccent));
}

void NowPlayingPanel::setPosition(qint64 positionMs)
{
    m_art->setPosition(positionMs);
    if (m_seek->isSliderDown())
        return;
    {
        const QSignalBlocker blocker(m_seek);
        m_seek->setValue(int(qMin<qint64>(positionMs, m_seek->maximum())));
    }
    updateTimeLabels(positionMs);
}

void NowPlayingPanel::setDuration(qint64 durationMs)
{
    m_duration = durationMs;
    m_seek->setRange(0, int(qMax<qint64>(0, durationMs)));
    m_seek->setPageStep(int(qBound<qint64>(qint64(1000), durationMs / 20, qint64(30000))));
    m_total->setText(Format::duration(durationMs));
}

void NowPlayingPanel::updateTimeLabels(qint64 positionMs)
{
    m_elapsed->setText(Format::duration(qMax<qint64>(positionMs, 0)).replace(QStringLiteral("--:--"),
                                                                              QStringLiteral("0:00")));
}

void NowPlayingPanel::setShuffle(bool on)
{
    const QSignalBlocker blocker(m_shuffle);
    m_shuffle->setChecked(on);
    m_shuffle->setToolTip(on ? tr("Shuffle: on (Ctrl+S)") : tr("Shuffle: off (Ctrl+S)"));
}

void NowPlayingPanel::setRepeat(PlayQueue::Repeat mode)
{
    const QSignalBlocker blocker(m_repeat);
    m_repeat->setChecked(mode != PlayQueue::Repeat::Off);
    m_repeat->setIcon(Icons::icon(mode == PlayQueue::Repeat::One ? Icons::Kind::RepeatOne
                                                                 : Icons::Kind::Repeat,
                                  Theme::textDim, Theme::accent));
    switch (mode) {
    case PlayQueue::Repeat::Off:
        m_repeat->setToolTip(tr("Repeat: off (Ctrl+R)"));
        break;
    case PlayQueue::Repeat::All:
        m_repeat->setToolTip(tr("Repeat: whole queue (Ctrl+R)"));
        break;
    case PlayQueue::Repeat::One:
        m_repeat->setToolTip(tr("Repeat: current track (Ctrl+R)"));
        break;
    }
}

void NowPlayingPanel::setVolume(int percent)
{
    const QSignalBlocker blocker(m_volume);
    m_volume->setValue(percent);
    m_volume->setToolTip(tr("Volume: %1%").arg(percent));
}

void NowPlayingPanel::setMuted(bool muted)
{
    m_muted = muted;
    updateVolumeIcon();
}

void NowPlayingPanel::updateVolumeIcon()
{
    m_mute->setIcon(Icons::icon(m_muted ? Icons::Kind::Muted : Icons::Kind::Volume,
                                m_muted ? Theme::accent : Theme::textDim));
    m_mute->setToolTip(m_muted ? tr("Unmute (M)") : tr("Mute (M)"));
}
