#pragma once

#include "LibraryView.h"

#include <QMainWindow>
#include <QUrl>

class Library;
class NowPlayingPanel;
class PlayQueue;
class PlayerController;
class QueueView;
class QAction;
class QLabel;
class QLineEdit;
class QProgressBar;
class QSplitter;
class QStackedWidget;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Queues files or folders given on the command line and starts playing.
    void openPaths(const QStringList &paths);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QWidget *buildLibraryPanel();
    QWidget *buildQueuePanel();
    void buildMenus();
    void connectPlayer();

    void chooseLibraryFolder();
    void addFilesToQueue();
    void enqueue(const QVector<Track> &tracks, LibraryView::Action action, int row = -1);
    QVector<Track> tracksFromUrls(const QList<QUrl> &urls) const;
    void cycleRepeat();

    void onScanStarted();
    void onScanFinished();
    void updateQueueInfo();
    void updateWindowTitle();
    void updateLibraryPlaceholder();

    void restoreSession();
    void saveSession();

    Library *m_library;
    PlayQueue *m_queue;
    PlayerController *m_player;

    QSplitter *m_splitter = nullptr;
    LibraryView *m_libraryView = nullptr;
    QStackedWidget *m_libraryStack = nullptr;
    QLabel *m_libraryHint = nullptr;
    QLabel *m_libraryInfo = nullptr;
    QLineEdit *m_search = nullptr;
    QueueView *m_queueView = nullptr;
    QLabel *m_queueInfo = nullptr;
    NowPlayingPanel *m_nowPlaying = nullptr;
    QProgressBar *m_scanProgress = nullptr;
    QLabel *m_libraryStats = nullptr;

    QAction *m_playAction = nullptr;
    QAction *m_shuffleAction = nullptr;
    QAction *m_repeatAction = nullptr;
    QAction *m_lyricsAction = nullptr;
    QAction *m_muteAction = nullptr;
};
