#include "Library.h"

#include "ImageUtils.h"
#include "MetadataReader.h"

#include <QCollator>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace {

constexpr quint32 kCacheMagic = 0x414d4252; // "AMBR"
constexpr quint32 kCacheVersion = 1;
constexpr int kThumbnailSize = 128;

struct CacheEntry {
    qint64 mtime = 0;
    qint64 size = 0;
    Track track;
};

QString cacheDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

QString tagCachePath()
{
    return cacheDir() + QStringLiteral("/library.cache");
}

QHash<QString, CacheEntry> loadTagCache()
{
    QHash<QString, CacheEntry> cache;
    QFile file(tagCachePath());
    if (!file.open(QIODevice::ReadOnly))
        return cache;
    QDataStream in(&file);
    quint32 magic = 0, version = 0;
    qint32 count = 0;
    in >> magic >> version >> count;
    if (magic != kCacheMagic || version != kCacheVersion || count < 0)
        return cache;
    cache.reserve(count);
    for (qint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i) {
        CacheEntry entry;
        in >> entry.mtime >> entry.size >> entry.track;
        cache.insert(entry.track.path, entry);
    }
    if (in.status() != QDataStream::Ok)
        cache.clear();
    return cache;
}

void saveTagCache(const QHash<QString, CacheEntry> &cache)
{
    QDir().mkpath(cacheDir());
    QSaveFile file(tagCachePath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    QDataStream out(&file);
    out << kCacheMagic << kCacheVersion << qint32(cache.size());
    for (const CacheEntry &entry : cache)
        out << entry.mtime << entry.size << entry.track;
    file.commit();
}

QString albumKey(const Track &t)
{
    // Group by album title plus album artist; without an album artist, tracks
    // of the same album title in the same folder belong together, which keeps
    // compilations in one piece.
    const QString owner = t.albumArtist.isEmpty() ? QFileInfo(t.path).absolutePath()
                                                  : t.albumArtist.toLower();
    return t.album.toLower() + QChar(0x1f) + owner;
}

QString thumbnailCachePath(const QString &albumKey, const QString &firstTrack)
{
    const QFileInfo info(firstTrack);
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(albumKey.toUtf8());
    hash.addData(firstTrack.toUtf8());
    hash.addData(QByteArray::number(info.lastModified().toMSecsSinceEpoch()));
    return cacheDir() + QStringLiteral("/thumbs/") + QString::fromLatin1(hash.result().toHex());
}

} // namespace

qint64 Album::durationMs() const
{
    qint64 total = 0;
    for (const Track &t : tracks)
        total += t.durationMs;
    return total;
}

Library::Library(QObject *parent)
    : QObject(parent)
{
    connect(&m_scanWatcher, &QFutureWatcher<ScanResult>::finished, this, &Library::onScanFinished);
}

Library::~Library()
{
    cancelWork();
    m_scanWatcher.waitForFinished();
    m_thumbFuture.waitForFinished();
}

Track Library::track(const QString &path) const
{
    const auto it = m_byPath.constFind(path);
    if (it != m_byPath.constEnd())
        return *it;
    return Metadata::readTrack(path);
}

QStringList Library::collectFiles(const QString &directory)
{
    QStringList files;
    QDirIterator it(directory, Formats::nameFilters(), QDir::Files | QDir::Readable,
                    QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while (it.hasNext())
        files << it.next();
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(files.begin(), files.end(), collator);
    return files;
}

void Library::cancelWork()
{
    if (m_cancel)
        m_cancel->store(true);
}

void Library::scan(const QString &rootPath)
{
    if (rootPath.isEmpty())
        return;
    cancelWork();
    m_scanWatcher.waitForFinished();

    m_root = rootPath;
    m_cancel = std::make_shared<std::atomic_bool>(false);
    ++m_generation;
    emit scanStarted();
    m_scanWatcher.setFuture(QtConcurrent::run(
        [this, root = rootPath, cancel = m_cancel] { return runScan(root, cancel); }));
}

void Library::rescan()
{
    scan(m_root);
}

Library::ScanResult Library::runScan(const QString &root, std::shared_ptr<std::atomic_bool> cancel)
{
    ScanResult result;
    result.root = root;

    const QHash<QString, CacheEntry> oldCache = loadTagCache();
    QHash<QString, CacheEntry> newCache = oldCache; // entries of other roots survive
    QVector<Track> tracks;

    QDirIterator it(root, Formats::nameFilters(), QDir::Files | QDir::Readable,
                    QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    int seen = 0;
    while (it.hasNext()) {
        if (cancel->load())
            return result;
        const QString path = it.next();
        const QFileInfo info = it.fileInfo();
        const qint64 mtime = info.lastModified().toMSecsSinceEpoch();
        const qint64 size = info.size();

        const auto cached = oldCache.constFind(path);
        if (cached != oldCache.constEnd() && cached->mtime == mtime && cached->size == size) {
            tracks.push_back(cached->track);
        } else {
            CacheEntry entry{mtime, size, Metadata::readTrack(path)};
            tracks.push_back(entry.track);
            newCache.insert(path, entry);
        }

        if (++seen % 25 == 0)
            QMetaObject::invokeMethod(this, [this, seen] { emit scanProgress(seen); },
                                      Qt::QueuedConnection);
    }

    // Drop cache entries for files below this root that no longer exist.
    const QString prefix = QDir(root).absolutePath() + QLatin1Char('/');
    for (auto c = newCache.begin(); c != newCache.end();) {
        if (c.key().startsWith(prefix) && !QFileInfo::exists(c.key()))
            c = newCache.erase(c);
        else
            ++c;
    }
    saveTagCache(newCache);

    // Group into albums.
    QHash<QString, int> indexByKey;
    for (const Track &t : std::as_const(tracks)) {
        const QString key = albumKey(t);
        auto found = indexByKey.constFind(key);
        if (found == indexByKey.constEnd()) {
            Album album;
            album.key = key;
            album.title = t.displayAlbum();
            found = indexByKey.insert(key, int(result.albums.size()));
            result.albums.push_back(album);
        }
        result.albums[*found].tracks.push_back(t);
    }

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    for (Album &album : result.albums) {
        std::sort(album.tracks.begin(), album.tracks.end(), [&](const Track &a, const Track &b) {
            if (a.discNumber != b.discNumber)
                return a.discNumber < b.discNumber;
            if (a.trackNumber != b.trackNumber)
                return a.trackNumber < b.trackNumber;
            return collator.compare(a.path, b.path) < 0;
        });

        QString artist;
        bool various = false;
        for (const Track &t : std::as_const(album.tracks)) {
            if (!t.albumArtist.isEmpty()) {
                artist = t.albumArtist;
                various = false;
                break;
            }
            if (artist.isEmpty())
                artist = t.artist;
            else if (!t.artist.isEmpty() && t.artist.compare(artist, Qt::CaseInsensitive) != 0)
                various = true;
        }
        for (const Track &t : std::as_const(album.tracks))
            album.year = qMax(album.year, t.year);
        album.artist = various ? QStringLiteral("Various Artists")
                               : (artist.isEmpty() ? QStringLiteral("Unknown Artist") : artist);
    }

    std::sort(result.albums.begin(), result.albums.end(), [&](const Album &a, const Album &b) {
        const int byArtist = collator.compare(a.artist, b.artist);
        if (byArtist != 0)
            return byArtist < 0;
        if (a.year != b.year)
            return a.year < b.year;
        return collator.compare(a.title, b.title) < 0;
    });

    return result;
}

void Library::onScanFinished()
{
    ScanResult result = m_scanWatcher.result();
    if (m_cancel && m_cancel->load())
        return; // superseded by a newer scan

    m_albums = std::move(result.albums);
    m_byPath.clear();
    QVector<ThumbJob> jobs;
    jobs.reserve(m_albums.size());
    for (const Album &album : std::as_const(m_albums)) {
        ThumbJob job{album.key, {}};
        for (const Track &t : album.tracks) {
            m_byPath.insert(t.path, t);
            if (job.paths.size() < 3)
                job.paths.push_back(t.path);
        }
        jobs.push_back(job);
    }
    emit scanFinished();

    m_thumbFuture = QtConcurrent::run(
        [this, jobs = std::move(jobs), cancel = m_cancel, generation = m_generation]() mutable {
            runThumbnails(std::move(jobs), cancel, generation);
        });
}

void Library::runThumbnails(QVector<ThumbJob> jobs, std::shared_ptr<std::atomic_bool> cancel,
                            quint64 generation)
{
    QDir().mkpath(cacheDir() + QStringLiteral("/thumbs"));
    for (const ThumbJob &job : jobs) {
        if (cancel->load())
            return;
        if (job.paths.isEmpty())
            continue;

        const QString cached = thumbnailCachePath(job.key, job.paths.first());
        QImage thumb;
        if (QFileInfo::exists(cached + QStringLiteral(".none"))) {
            continue;
        } else if (QFileInfo::exists(cached + QStringLiteral(".jpg"))) {
            thumb.load(cached + QStringLiteral(".jpg"));
        } else {
            QImage cover = Metadata::readEmbeddedCover(job.paths.first());
            if (cover.isNull())
                cover = Metadata::findFolderCover(QFileInfo(job.paths.first()).absolutePath());
            for (int i = 1; cover.isNull() && i < job.paths.size(); ++i)
                cover = Metadata::readEmbeddedCover(job.paths[i]);
            thumb = ImageUtils::squareThumbnail(cover, kThumbnailSize);
            if (thumb.isNull()) {
                QFile marker(cached + QStringLiteral(".none"));
                if (marker.open(QIODevice::WriteOnly))
                    marker.close();
            } else {
                thumb.convertToFormat(QImage::Format_RGB32).save(cached + QStringLiteral(".jpg"), "JPG", 90);
            }
        }

        if (!thumb.isNull()) {
            QMetaObject::invokeMethod(
                this,
                [this, key = job.key, thumb, generation] {
                    if (generation == m_generation)
                        emit thumbnailReady(key, thumb);
                },
                Qt::QueuedConnection);
        }
    }
}
