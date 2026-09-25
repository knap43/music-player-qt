#include "Library.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

namespace {

QByteArray bigEndian32(quint32 v)
{
    return QByteArray(1, char(v >> 24)) + char(v >> 16) + char(v >> 8) + char(v);
}

QByteArray textFrame(const char *id, const QByteArray &latin1)
{
    const QByteArray body = QByteArray(1, '\0') + latin1; // encoding: ISO-8859-1
    return QByteArray(id) + bigEndian32(quint32(body.size())) + QByteArray(2, '\0') + body;
}

// A tiny but valid MP3: an ID3v2.3 tag followed by a few silent MPEG frames.
void writeMp3(const QString &path, const QByteArray &title, const QByteArray &album)
{
    const QByteArray frames = textFrame("TIT2", title) + textFrame("TALB", album)
                              + textFrame("TPE1", "Tagged Artist");
    const quint32 size = quint32(frames.size());
    QByteArray tag("ID3\x03\x00\x00", 6);
    for (int shift : {21, 14, 7, 0}) // syncsafe integer
        tag += char((size >> shift) & 0x7f);
    tag += frames;

    // MPEG-1 Layer III, 128 kbit/s, 44.1 kHz: 417-byte frames.
    QByteArray audio;
    for (int i = 0; i < 8; ++i)
        audio += QByteArray("\xff\xfb\x90\x64", 4) + QByteArray(413, '\0');

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(tag + audio);
}

void touch(const QString &path)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
}

} // namespace

class TestLibrary : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void foldersAreAlbums()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QDir dir(root.path());
        for (const char *sub : {"Artist A/Same Name", "Artist B/Same Name", "Mixed", "Mixed/CD2", "Empty"})
            QVERIFY(dir.mkpath(QString::fromLatin1(sub)));

        // Identical album tags in two folders: still two albums.
        writeMp3(dir.filePath("Artist A/Same Name/1.mp3"), "One", "Shared Tag");
        writeMp3(dir.filePath("Artist B/Same Name/1.mp3"), "One", "Shared Tag");
        // Different album tags in one folder: still one album, in file-name order.
        writeMp3(dir.filePath("Mixed/10 - ten.mp3"), "Ten", "Album X");
        writeMp3(dir.filePath("Mixed/2 - two.mp3"), "Two", "Album Y");
        touch(dir.filePath("Mixed/1 - one.flac"));
        // A sub-folder is an album of its own.
        touch(dir.filePath("Mixed/CD2/a.webm"));
        // Files directly in the root form an album too; other files are ignored.
        touch(dir.filePath("loose.mp3"));
        touch(dir.filePath("Empty/notes.txt"));

        Library library;
        QSignalSpy finished(&library, &Library::scanFinished);
        library.scan(root.path());
        QVERIFY(finished.wait(10000));

        const QVector<Album> &albums = library.albums();
        QStringList keys;
        for (const Album &a : albums)
            keys << QDir(root.path()).relativeFilePath(a.key);
        QCOMPARE(keys, (QStringList{".", "Artist A/Same Name", "Artist B/Same Name", "Mixed", "Mixed/CD2"}));

        QCOMPARE(albums[1].title, QStringLiteral("Same Name"));
        QCOMPARE(albums[1].location, QStringLiteral("Artist A"));
        QCOMPARE(albums[2].location, QStringLiteral("Artist B"));
        QCOMPARE(albums[0].title, QDir(root.path()).dirName());

        const Album &mixed = albums[3];
        QCOMPARE(mixed.title, QStringLiteral("Mixed"));
        QCOMPARE(mixed.tracks.size(), 3);
        QCOMPARE(QFileInfo(mixed.tracks[0].path).fileName(), QStringLiteral("1 - one.flac"));
        QCOMPARE(QFileInfo(mixed.tracks[1].path).fileName(), QStringLiteral("2 - two.mp3"));
        QCOMPARE(QFileInfo(mixed.tracks[2].path).fileName(), QStringLiteral("10 - ten.mp3"));

        // Tags are still used for the track itself, never for the album.
        QCOMPARE(mixed.tracks[1].title, QStringLiteral("Two"));
        QCOMPARE(mixed.tracks[1].artist, QStringLiteral("Tagged Artist"));
        QCOMPARE(mixed.tracks[1].displayAlbum(), QStringLiteral("Mixed"));
        QCOMPARE(mixed.tracks[2].displayAlbum(), QStringLiteral("Mixed"));
    }
};

QTEST_GUILESS_MAIN(TestLibrary)
#include "tst_library.moc"
