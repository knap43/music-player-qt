#include "Track.h"

#include <QDir>
#include <QFileInfo>

QString Track::displayTitle() const
{
    return title.isEmpty() ? QFileInfo(path).completeBaseName() : title;
}

QString Track::displayArtist() const
{
    return artist.isEmpty() ? QStringLiteral("Unknown Artist") : artist;
}

QString Track::displayAlbum() const
{
    return QFileInfo(path).dir().dirName();
}

QDataStream &operator<<(QDataStream &out, const Track &t)
{
    out << t.path << t.title << t.artist << qint32(t.year) << qint32(t.trackNumber) << qint32(t.discNumber)
        << qint64(t.durationMs) << t.tagged;
    return out;
}

QDataStream &operator>>(QDataStream &in, Track &t)
{
    qint32 year = 0, trackNumber = 0, discNumber = 0;
    qint64 duration = 0;
    in >> t.path >> t.title >> t.artist >> year >> trackNumber >> discNumber >> duration >> t.tagged;
    t.year = year;
    t.trackNumber = trackNumber;
    t.discNumber = discNumber;
    t.durationMs = duration;
    return in;
}

namespace Formats {

const QStringList &suffixes()
{
    static const QStringList list{QStringLiteral("mp3"), QStringLiteral("flac"),
                                  QStringLiteral("webm")};
    return list;
}

QStringList nameFilters()
{
    QStringList filters;
    for (const QString &suffix : suffixes())
        filters << QStringLiteral("*.") + suffix;
    return filters;
}

bool isSupported(const QString &path)
{
    return suffixes().contains(QFileInfo(path).suffix().toLower());
}

} // namespace Formats

namespace Format {

QString duration(qint64 ms)
{
    if (ms <= 0)
        return QStringLiteral("--:--");
    const qint64 totalSeconds = (ms + 500) / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds / 60) % 60;
    const qint64 seconds = totalSeconds % 60;
    if (hours > 0)
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0'));
}

} // namespace Format
