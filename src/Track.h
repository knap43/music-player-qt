#pragma once

#include <QDataStream>
#include <QMetaType>
#include <QString>
#include <QStringList>

// Everything the player needs to know about a single audio file.
struct Track {
    QString path;
    QString title;
    QString artist;
    int year = 0;
    int trackNumber = 0;
    int discNumber = 0;
    qint64 durationMs = 0;
    // False when TagLib could not read the file and the fields above were
    // guessed from the file name; the player then trusts the stream's own
    // metadata once playback starts.
    bool tagged = false;

    bool isValid() const { return !path.isEmpty(); }
    QString displayTitle() const;
    QString displayArtist() const;
    // Albums are folders: this is the name of the folder holding the file,
    // whatever the album tag says.
    QString displayAlbum() const;
};

Q_DECLARE_METATYPE(Track)

QDataStream &operator<<(QDataStream &out, const Track &track);
QDataStream &operator>>(QDataStream &in, Track &track);

namespace Formats {
// Lower-case file suffixes the player accepts.
const QStringList &suffixes();
// Glob patterns for QDir / QFileDialog ("*.mp3", ...).
QStringList nameFilters();
bool isSupported(const QString &path);
} // namespace Formats

namespace Format {
// 3:07, 1:02:45; "--:--" for unknown durations.
QString duration(qint64 ms);
} // namespace Format
