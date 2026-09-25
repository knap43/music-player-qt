#include "MainWindow.h"

#include "Icons.h"
#include "Library.h"
#include "MetadataReader.h"
#include "NowPlayingPanel.h"
#include "PlayQueue.h"
#include "PlayerController.h"
#include "QueueView.h"
#include "Theme.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QVBoxLayout>

namespace {

QWidget *makePanelHeader(const QString &title, QLabel *info, QWidget *parent)
{
    auto *header = new QWidget(parent);
    auto *layout = new QHBoxLayout(header);
    layout->setContentsMargins(14, 12, 14, 8);
    auto *label = new QLabel(title, header);
    label->setObjectName(QStringLiteral("panelTitle"));
    info->setObjectName(QStringLiteral("panelInfo"));
    info->setParent(header);
    layout->addWidget(label);
    layout->addStretch(1);
    layout->addWidget(info);
    return header;
}

QString trackCount(int n)
{
    return n == 1 ? MainWindow::tr("1 track") : MainWindow::tr("%1 tracks").arg(n);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_library(new Library(this))
    , m_queue(new PlayQueue(this))
    , m_player(new PlayerController(m_queue, this))
{
    setWindowIcon(Icons::appIcon());
    setAcceptDrops(false);

    m_nowPlaying = new NowPlayingPanel(this);
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(2);
    m_splitter->addWidget(buildLibraryPanel());
    m_splitter->addWidget(buildQueuePanel());
    m_splitter->addWidget(m_nowPlaying);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 0);
    m_splitter->setStretchFactor(2, 1);
    m_splitter->setSizes({340, 380, 560});
    setCentralWidget(m_splitter);

    m_scanProgress = new QProgressBar(this);
    m_scanProgress->setRange(0, 0);
    m_scanProgress->setTextVisible(false);
    m_scanProgress->setFixedWidth(140);
    m_scanProgress->hide();
    m_libraryStats = new QLabel(this);
    statusBar()->addPermanentWidget(m_scanProgress);
    statusBar()->addPermanentWidget(m_libraryStats);
    statusBar()->setSizeGripEnabled(false);

    buildMenus();
    connectPlayer();

    connect(m_library, &Library::scanStarted, this, &MainWindow::onScanStarted);
    connect(m_library, &Library::scanProgress, this, [this](int files) {
        m_libraryStats->setText(tr("Scanning… %1 files").arg(files));
    });
    connect(m_library, &Library::scanFinished, this, &MainWindow::onScanFinished);
    connect(m_library, &Library::thumbnailReady, m_libraryView, &LibraryView::setThumbnail);

    resize(1320, 820);
    restoreSession();
    updateQueueInfo();
    updateWindowTitle();
}

MainWindow::~MainWindow() = default;

QWidget *MainWindow::buildLibraryPanel()
{
    auto *panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("panel"));
    panel->setAttribute(Qt::WA_StyledBackground);
    panel->setMinimumWidth(270);

    m_libraryInfo = new QLabel;
    m_search = new QLineEdit(panel);
    m_search->setPlaceholderText(tr("Search albums, artists, tracks"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(Icons::icon(Icons::Kind::Search, Theme::textFaint), QLineEdit::LeadingPosition);
    auto *clearSearch = new QShortcut(QKeySequence(Qt::Key_Escape), m_search);
    clearSearch->setContext(Qt::WidgetShortcut);
    connect(clearSearch, &QShortcut::activated, m_search, &QLineEdit::clear);

    m_libraryView = new LibraryView(panel);

    // Shown instead of the tree while there is nothing to list.
    auto *empty = new QWidget(panel);
    auto *emptyLayout = new QVBoxLayout(empty);
    emptyLayout->setContentsMargins(24, 24, 24, 24);
    m_libraryHint = new QLabel(empty);
    m_libraryHint->setObjectName(QStringLiteral("emptyHint"));
    m_libraryHint->setAlignment(Qt::AlignCenter);
    m_libraryHint->setWordWrap(true);
    auto *choose = new QPushButton(tr("Choose Music Folder…"), empty);
    choose->setObjectName(QStringLiteral("accentButton"));
    choose->setCursor(Qt::PointingHandCursor);
    connect(choose, &QPushButton::clicked, this, &MainWindow::chooseLibraryFolder);
    emptyLayout->addStretch(1);
    emptyLayout->addWidget(m_libraryHint);
    emptyLayout->addSpacing(12);
    emptyLayout->addWidget(choose, 0, Qt::AlignHCenter);
    emptyLayout->addStretch(1);

    m_libraryStack = new QStackedWidget(panel);
    m_libraryStack->addWidget(empty);
    m_libraryStack->addWidget(m_libraryView);

    auto *addButton = new QPushButton(tr("Add to Queue"), panel);
    auto *playButton = new QPushButton(tr("Play"), panel);
    playButton->setObjectName(QStringLiteral("accentButton"));
    for (QPushButton *b : {addButton, playButton}) {
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
    }
    addButton->setToolTip(tr("Append the selected albums or tracks to the queue (Enter)"));
    playButton->setToolTip(tr("Play the selection now (Ctrl+Enter)"));
    connect(addButton, &QPushButton::clicked, this, [this] {
        enqueue(m_libraryView->selectedTracks(), LibraryView::Action::Append);
    });
    connect(playButton, &QPushButton::clicked, this, [this] {
        enqueue(m_libraryView->selectedTracks(), LibraryView::Action::PlayNow);
    });
    auto updateButtons = [this, addButton, playButton] {
        const bool any = !m_libraryView->selectedItems().isEmpty();
        addButton->setEnabled(any);
        playButton->setEnabled(any);
    };
    connect(m_libraryView, &QTreeWidget::itemSelectionChanged, this, updateButtons);
    updateButtons();

    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(12, 10, 12, 12);
    buttons->setSpacing(8);
    buttons->addWidget(addButton, 1);
    buttons->addWidget(playButton, 1);

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(makePanelHeader(tr("LIBRARY"), m_libraryInfo, panel));
    auto *searchRow = new QHBoxLayout;
    searchRow->setContentsMargins(12, 0, 12, 10);
    searchRow->addWidget(m_search);
    layout->addLayout(searchRow);
    layout->addWidget(m_libraryStack, 1);
    layout->addLayout(buttons);

    connect(m_search, &QLineEdit::textChanged, m_libraryView, &LibraryView::setFilter);
    connect(m_libraryView, &LibraryView::tracksRequested, this,
            [this](const QVector<Track> &tracks, LibraryView::Action action) { enqueue(tracks, action); });

    updateLibraryPlaceholder();
    return panel;
}

QWidget *MainWindow::buildQueuePanel()
{
    auto *panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("panel"));
    panel->setAttribute(Qt::WA_StyledBackground);
    panel->setMinimumWidth(280);

    m_queueInfo = new QLabel;
    m_queueView = new QueueView(m_queue, panel);

    auto *removeButton = new QPushButton(tr("Remove"), panel);
    auto *clearButton = new QPushButton(tr("Clear"), panel);
    for (QPushButton *b : {removeButton, clearButton}) {
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
    }
    removeButton->setToolTip(tr("Remove the selected tracks from the queue (Delete)"));
    clearButton->setToolTip(tr("Remove everything from the queue"));
    connect(removeButton, &QPushButton::clicked, m_queueView, &QueueView::removeSelected);
    connect(clearButton, &QPushButton::clicked, m_queue, &PlayQueue::clear);
    auto updateButtons = [this, removeButton, clearButton] {
        removeButton->setEnabled(m_queueView->selectionModel()->hasSelection());
        clearButton->setEnabled(!m_queue->isEmpty());
    };
    connect(m_queueView->selectionModel(), &QItemSelectionModel::selectionChanged, this, updateButtons);
    connect(m_queue, &QAbstractItemModel::modelReset, this, updateButtons);
    connect(m_queue, &PlayQueue::contentsChanged, this, updateButtons);
    updateButtons();

    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(12, 10, 12, 12);
    buttons->setSpacing(8);
    buttons->addWidget(removeButton, 1);
    buttons->addWidget(clearButton, 1);

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(makePanelHeader(tr("QUEUE"), m_queueInfo, panel));
    layout->addWidget(m_queueView, 1);
    layout->addLayout(buttons);

    connect(m_queueView, &QueueView::rowActivated, m_player, &PlayerController::playRow);
    connect(m_queueView, &QueueView::trackPathsDropped, this, [this](const QStringList &paths, int row) {
        QVector<Track> tracks;
        for (const QString &path : paths)
            tracks << m_library->track(path);
        enqueue(tracks, LibraryView::Action::Append, row);
    });
    connect(m_queueView, &QueueView::urlsDropped, this, [this](const QList<QUrl> &urls, int row) {
        enqueue(tracksFromUrls(urls), LibraryView::Action::Append, row);
    });
    connect(m_queue, &PlayQueue::contentsChanged, this, &MainWindow::updateQueueInfo);
    connect(m_queue, &PlayQueue::currentRowChanged, this, [this](int row) {
        if (row >= 0)
            m_queueView->scrollTo(m_queue->index(row), QAbstractItemView::EnsureVisible);
    });

    return panel;
}

void MainWindow::buildMenus()
{
    // File
    QMenu *file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("&Open Music Folder…"), QKeySequence::Open, this, &MainWindow::chooseLibraryFolder);
    QAction *rescan = file->addAction(tr("&Rescan Library"), QKeySequence(Qt::Key_F5), m_library, &Library::rescan);
    connect(m_library, &Library::scanStarted, rescan, [rescan] { rescan->setEnabled(false); });
    connect(m_library, &Library::scanFinished, rescan, [rescan] { rescan->setEnabled(true); });
    file->addSeparator();
    file->addAction(tr("&Add Files to Queue…"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O), this,
                    &MainWindow::addFilesToQueue);
    file->addSeparator();
    file->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);

    // Playback
    QMenu *playback = menuBar()->addMenu(tr("&Playback"));
    m_playAction = playback->addAction(tr("&Play"), QKeySequence(Qt::Key_Space), m_player,
                                       &PlayerController::togglePlayPause);
    playback->addAction(tr("&Next Track"), QKeySequence(Qt::CTRL | Qt::Key_Right), m_player,
                        &PlayerController::next);
    playback->addAction(tr("P&revious Track"), QKeySequence(Qt::CTRL | Qt::Key_Left), m_player,
                        &PlayerController::previous);
    playback->addAction(tr("S&top"), QKeySequence(Qt::CTRL | Qt::Key_Period), m_player, &PlayerController::stop);
    playback->addSeparator();
    playback->addAction(tr("Seek &Forward 10 s"), QKeySequence(Qt::SHIFT | Qt::Key_Right), this,
                        [this] { m_player->seek(m_player->position() + 10000); });
    playback->addAction(tr("Seek &Backward 10 s"), QKeySequence(Qt::SHIFT | Qt::Key_Left), this,
                        [this] { m_player->seek(qMax<qint64>(0, m_player->position() - 10000)); });
    playback->addSeparator();
    m_shuffleAction = playback->addAction(tr("&Shuffle"));
    m_shuffleAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    m_shuffleAction->setCheckable(true);
    connect(m_shuffleAction, &QAction::toggled, m_queue, &PlayQueue::setShuffle);
    m_repeatAction = playback->addAction(tr("&Repeat: Off"), QKeySequence(Qt::CTRL | Qt::Key_R), this,
                                         &MainWindow::cycleRepeat);
    playback->addSeparator();
    playback->addAction(tr("Volume &Up"), QKeySequence(Qt::CTRL | Qt::Key_Up), this,
                        [this] { m_player->setVolume(m_player->volume() + 5); });
    playback->addAction(tr("Volume &Down"), QKeySequence(Qt::CTRL | Qt::Key_Down), this,
                        [this] { m_player->setVolume(m_player->volume() - 5); });
    m_muteAction = playback->addAction(tr("&Mute"));
    m_muteAction->setShortcut(QKeySequence(Qt::Key_M));
    m_muteAction->setCheckable(true);
    connect(m_muteAction, &QAction::toggled, m_player, &PlayerController::setMuted);

    // View
    QMenu *view = menuBar()->addMenu(tr("&View"));
    m_lyricsAction = view->addAction(tr("Show &Lyrics"));
    m_lyricsAction->setShortcut(QKeySequence(Qt::Key_L));
    m_lyricsAction->setCheckable(true);
    m_lyricsAction->setChecked(true);
    connect(m_lyricsAction, &QAction::toggled, m_nowPlaying, &NowPlayingPanel::setLyricsEnabled);
    connect(m_nowPlaying, &NowPlayingPanel::lyricsToggled, m_lyricsAction, &QAction::setChecked);
    view->addAction(tr("&Find in Library"), QKeySequence::Find, this, [this] {
        m_search->setFocus();
        m_search->selectAll();
    });

    // Queue
    QMenu *queue = menuBar()->addMenu(tr("&Queue"));
    queue->addAction(tr("&Remove Selected"), m_queueView, &QueueView::removeSelected);
    queue->addAction(tr("&Clear Queue"), m_queue, &PlayQueue::clear);

    // Help
    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("&About"), this, [this] {
        QMessageBox::about(
            this, tr("About Amber"),
            tr("<h3>Amber</h3>"
               "<p>A small player for local MP3, FLAC and WebM files.</p>"
               "<p><b>Library:</b> double-click or press Enter to queue an album or track, "
               "Ctrl+Enter to play it now, or drag it onto the queue. Right-click for more.</p>"
               "<p><b>Lyrics:</b> read from a matching <i>.lrc</i> file next to the track, "
               "or from embedded tags. Click a synced line to jump to it.</p>"
               "<p><b>Keys:</b> Space play/pause · Ctrl+←/→ previous/next · Shift+←/→ seek · "
               "Ctrl+S shuffle · Ctrl+R repeat · Ctrl+↑/↓ volume · M mute · L lyrics · "
               "Ctrl+F search · Delete removes from the queue.</p>"));
    });
}

void MainWindow::connectPlayer()
{
    NowPlayingPanel *np = m_nowPlaying;

    connect(np, &NowPlayingPanel::playPauseClicked, m_player, &PlayerController::togglePlayPause);
    connect(np, &NowPlayingPanel::nextClicked, m_player, &PlayerController::next);
    connect(np, &NowPlayingPanel::previousClicked, m_player, &PlayerController::previous);
    connect(np, &NowPlayingPanel::seekRequested, m_player, &PlayerController::seek);
    connect(np, &NowPlayingPanel::volumeChangeRequested, m_player, &PlayerController::setVolume);
    connect(np, &NowPlayingPanel::muteToggled, m_player, &PlayerController::setMuted);
    connect(np, &NowPlayingPanel::shuffleToggled, m_queue, &PlayQueue::setShuffle);
    connect(np, &NowPlayingPanel::repeatClicked, this, &MainWindow::cycleRepeat);

    connect(m_player, &PlayerController::trackChanged, np, &NowPlayingPanel::setTrack);
    connect(m_player, &PlayerController::trackChanged, this, &MainWindow::updateWindowTitle);
    connect(m_player, &PlayerController::coverChanged, np, &NowPlayingPanel::setCover);
    connect(m_player, &PlayerController::lyricsChanged, np, &NowPlayingPanel::setLyrics);
    connect(m_player, &PlayerController::positionChanged, np, &NowPlayingPanel::setPosition);
    connect(m_player, &PlayerController::durationChanged, np, &NowPlayingPanel::setDuration);
    connect(m_player, &PlayerController::volumeChanged, np, &NowPlayingPanel::setVolume);
    connect(m_player, &PlayerController::mutedChanged, np, &NowPlayingPanel::setMuted);
    connect(m_player, &PlayerController::mutedChanged, m_muteAction, &QAction::setChecked);
    connect(m_player, &PlayerController::playingChanged, np, &NowPlayingPanel::setPlaying);
    connect(m_player, &PlayerController::playingChanged, m_queueView, &QueueView::setPlaying);
    connect(m_player, &PlayerController::playingChanged, this, [this](bool playing) {
        m_playAction->setText(playing ? tr("&Pause") : tr("&Play"));
        updateWindowTitle();
    });
    connect(m_player, &PlayerController::errorOccurred, this, [this](const QString &message) {
        statusBar()->showMessage(message, 8000);
    });

    connect(m_queue, &PlayQueue::shuffleChanged, np, &NowPlayingPanel::setShuffle);
    connect(m_queue, &PlayQueue::shuffleChanged, m_shuffleAction, &QAction::setChecked);
    connect(m_queue, &PlayQueue::repeatChanged, np, &NowPlayingPanel::setRepeat);
    connect(m_queue, &PlayQueue::repeatChanged, this, [this](PlayQueue::Repeat mode) {
        switch (mode) {
        case PlayQueue::Repeat::Off:
            m_repeatAction->setText(tr("&Repeat: Off"));
            break;
        case PlayQueue::Repeat::All:
            m_repeatAction->setText(tr("&Repeat: Queue"));
            break;
        case PlayQueue::Repeat::One:
            m_repeatAction->setText(tr("&Repeat: Track"));
            break;
        }
    });
}

void MainWindow::cycleRepeat()
{
    switch (m_queue->repeat()) {
    case PlayQueue::Repeat::Off:
        m_queue->setRepeat(PlayQueue::Repeat::All);
        break;
    case PlayQueue::Repeat::All:
        m_queue->setRepeat(PlayQueue::Repeat::One);
        break;
    case PlayQueue::Repeat::One:
        m_queue->setRepeat(PlayQueue::Repeat::Off);
        break;
    }
}

void MainWindow::chooseLibraryFolder()
{
    QString start = m_library->rootPath();
    if (start.isEmpty())
        start = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose Music Folder"), start);
    if (!dir.isEmpty())
        m_library->scan(dir);
}

void MainWindow::addFilesToQueue()
{
    const QString filter = tr("Audio files (%1)").arg(Formats::nameFilters().join(QLatin1Char(' ')));
    QString start = m_library->rootPath();
    if (start.isEmpty())
        start = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Add Files to Queue"), start, filter);
    QList<QUrl> urls;
    for (const QString &f : files)
        urls << QUrl::fromLocalFile(f);
    enqueue(tracksFromUrls(urls), LibraryView::Action::Append);
}

void MainWindow::openPaths(const QStringList &paths)
{
    QList<QUrl> urls;
    for (const QString &p : paths)
        urls << QUrl::fromLocalFile(QFileInfo(p).absoluteFilePath());
    const QVector<Track> tracks = tracksFromUrls(urls);
    if (!tracks.isEmpty())
        enqueue(tracks, LibraryView::Action::PlayNow);
}

QVector<Track> MainWindow::tracksFromUrls(const QList<QUrl> &urls) const
{
    QVector<Track> tracks;
    for (const QUrl &url : urls) {
        if (!url.isLocalFile())
            continue;
        const QString path = url.toLocalFile();
        const QFileInfo info(path);
        if (info.isDir()) {
            for (const QString &file : Library::collectFiles(path))
                tracks << m_library->track(file);
        } else if (info.isFile() && Formats::isSupported(path)) {
            tracks << m_library->track(info.absoluteFilePath());
        }
    }
    return tracks;
}

void MainWindow::enqueue(const QVector<Track> &tracks, LibraryView::Action action, int row)
{
    if (tracks.isEmpty())
        return;

    // Starting from silence, queueing something should simply start it.
    const bool idle = !m_player->hasTrack() && !m_player->isPlaying();

    switch (action) {
    case LibraryView::Action::Append: {
        const int first = row >= 0 ? m_queue->insert(row, tracks) : m_queue->append(tracks);
        statusBar()->showMessage(tr("Added %1 to the queue").arg(trackCount(int(tracks.size()))), 4000);
        if (idle)
            m_player->playRow(m_queue->shuffle() ? m_queue->advance(false) : first);
        break;
    }
    case LibraryView::Action::PlayNext:
        m_queue->insertAfterCurrent(tracks);
        statusBar()->showMessage(tr("%1 will play next").arg(trackCount(int(tracks.size()))), 4000);
        if (idle)
            m_player->next();
        break;
    case LibraryView::Action::PlayNow:
        m_player->playRow(m_queue->insertAfterCurrent(tracks));
        break;
    case LibraryView::Action::Replace:
        m_queue->clear();
        m_queue->append(tracks);
        m_player->next();
        break;
    }
}

void MainWindow::onScanStarted()
{
    m_scanProgress->show();
    m_libraryStats->setText(tr("Scanning…"));
    m_libraryHint->setText(tr("Scanning %1…").arg(QDir::toNativeSeparators(m_library->rootPath())));
}

void MainWindow::onScanFinished()
{
    m_scanProgress->hide();
    m_libraryView->setAlbums(m_library->albums());
    m_libraryView->setFilter(m_search->text());
    const int albums = int(m_library->albums().size());
    m_libraryInfo->setText(albums == 1 ? tr("1 album") : tr("%1 albums").arg(albums));
    m_libraryStats->setText(tr("%1 · %2").arg(m_libraryInfo->text(), trackCount(m_library->trackCount())));
    m_libraryStats->setToolTip(QDir::toNativeSeparators(m_library->rootPath()));
    updateLibraryPlaceholder();
}

void MainWindow::updateLibraryPlaceholder()
{
    const bool hasAlbums = !m_library->albums().isEmpty();
    m_libraryStack->setCurrentIndex(hasAlbums ? 1 : 0);
    if (hasAlbums || m_library->isScanning())
        return;
    if (m_library->rootPath().isEmpty())
        m_libraryHint->setText(tr("Pick the folder that holds your music.\nMP3, FLAC and WebM files are supported."));
    else
        m_libraryHint->setText(tr("No MP3, FLAC or WebM files were found in\n%1")
                                   .arg(QDir::toNativeSeparators(m_library->rootPath())));
}

void MainWindow::updateQueueInfo()
{
    if (m_queue->isEmpty()) {
        m_queueInfo->setText(tr("empty"));
        return;
    }
    m_queueInfo->setText(QStringLiteral("%1 · %2").arg(trackCount(m_queue->count()),
                                                       Format::duration(m_queue->totalDurationMs())));
}

void MainWindow::updateWindowTitle()
{
    if (!m_player->hasTrack()) {
        setWindowTitle(QStringLiteral("Amber"));
        return;
    }
    const Track &t = m_player->currentTrack();
    setWindowTitle(QStringLiteral("%1%2 — %3 · Amber")
                       .arg(m_player->isPlaying() ? QString() : tr("(paused) "), t.displayTitle(),
                            t.displayArtist()));
}

void MainWindow::restoreSession()
{
    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());
    m_splitter->restoreState(settings.value(QStringLiteral("window/splitter")).toByteArray());

    m_player->setVolume(settings.value(QStringLiteral("player/volume"), 80).toInt());
    m_nowPlaying->setVolume(m_player->volume());
    const bool lyrics = settings.value(QStringLiteral("view/lyrics"), true).toBool();
    m_lyricsAction->setChecked(lyrics);
    m_nowPlaying->setLyricsEnabled(lyrics);

    const QStringList paths = settings.value(QStringLiteral("queue/paths")).toStringList();
    QVector<Track> tracks;
    for (const QString &path : paths)
        if (QFileInfo::exists(path))
            tracks << Metadata::readTrack(path);
    m_queue->append(tracks);
    m_queue->setShuffle(settings.value(QStringLiteral("player/shuffle"), false).toBool());
    m_queue->setRepeat(PlayQueue::Repeat(qBound(0, settings.value(QStringLiteral("player/repeat"), 0).toInt(), 2)));

    const int row = settings.value(QStringLiteral("queue/current"), -1).toInt();
    if (row >= 0 && row < m_queue->count()) {
        m_queue->setCurrentRow(row);
        m_player->restoreCurrent(settings.value(QStringLiteral("queue/position"), 0).toLongLong());
        m_queueView->scrollTo(m_queue->index(row), QAbstractItemView::PositionAtCenter);
    }

    QString root = settings.value(QStringLiteral("library/root")).toString();
    if (root.isEmpty()) {
        const QString music = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
        if (QFileInfo(music).isDir() && music != QDir::homePath())
            root = music;
    }
    if (!root.isEmpty() && QFileInfo(root).isDir())
        m_library->scan(root);
    else
        updateLibraryPlaceholder();
}

void MainWindow::saveSession()
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/splitter"), m_splitter->saveState());
    settings.setValue(QStringLiteral("library/root"), m_library->rootPath());
    settings.setValue(QStringLiteral("player/volume"), m_player->volume());
    settings.setValue(QStringLiteral("player/shuffle"), m_queue->shuffle());
    settings.setValue(QStringLiteral("player/repeat"), int(m_queue->repeat()));
    settings.setValue(QStringLiteral("view/lyrics"), m_nowPlaying->lyricsEnabled());

    QStringList paths;
    for (const Track &t : m_queue->tracks())
        paths << t.path;
    settings.setValue(QStringLiteral("queue/paths"), paths);
    settings.setValue(QStringLiteral("queue/current"), m_queue->currentRow());
    settings.setValue(QStringLiteral("queue/position"), m_player->position());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSession();
    QMainWindow::closeEvent(event);
}
