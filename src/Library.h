#pragma once

#include "Track.h"

#include <QFutureWatcher>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QVector>

#include <atomic>
#include <memory>

struct Album {
    QString key;
    QString title;
    QString artist;
    int year = 0;
    QVector<Track> tracks;

    qint64 durationMs() const;
};

// Scans a music folder in the background and groups its files into albums.
// Tags are cached on disk (keyed by modification time and size), so rescans
// only have to read files that changed; album thumbnails are cached too.
class Library : public QObject
{
    Q_OBJECT
public:
    explicit Library(QObject *parent = nullptr);
    ~Library() override;

    QString rootPath() const { return m_root; }
    bool isScanning() const { return m_scanWatcher.isRunning(); }

    const QVector<Album> &albums() const { return m_albums; }
    int trackCount() const { return int(m_byPath.size()); }
    // Looks the track up in the library, falling back to reading its tags.
    Track track(const QString &path) const;

    // Recursively collects supported files below a directory, sorted.
    static QStringList collectFiles(const QString &directory);

public slots:
    void scan(const QString &rootPath);
    void rescan();

signals:
    void scanStarted();
    void scanProgress(int filesSeen);
    void scanFinished();
    void thumbnailReady(const QString &albumKey, const QImage &thumbnail);

private:
    struct ScanResult {
        QString root;
        QVector<Album> albums;
    };
    struct ThumbJob {
        QString key;
        QVector<QString> paths;
    };

    ScanResult runScan(const QString &root, std::shared_ptr<std::atomic_bool> cancel);
    void runThumbnails(QVector<ThumbJob> jobs, std::shared_ptr<std::atomic_bool> cancel,
                       quint64 generation);
    void onScanFinished();
    void cancelWork();

    QString m_root;
    QVector<Album> m_albums;
    QHash<QString, Track> m_byPath;
    QFutureWatcher<ScanResult> m_scanWatcher;
    QFuture<void> m_thumbFuture;
    std::shared_ptr<std::atomic_bool> m_cancel;
    quint64 m_generation = 0;
};
