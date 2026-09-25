#include "MetadataReader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringDecoder>

#include <attachedpictureframe.h>
#include <fileref.h>
#include <flacfile.h>
#include <flacpicture.h>
#include <id3v2tag.h>
#include <mpegfile.h>
#include <synchronizedlyricsframe.h>
#include <tag.h>
#include <taglib.h>
#include <tpropertymap.h>

namespace {

QString toQString(const TagLib::String &s)
{
    return QString::fromUtf8(s.toCString(true)).trimmed();
}

QByteArray toQByteArray(const TagLib::ByteVector &v)
{
    return QByteArray(v.data(), int(v.size()));
}

QString firstProperty(const TagLib::PropertyMap &props, std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        if (props.contains(key)) {
            const TagLib::StringList &values = props[key];
            if (!values.isEmpty())
                return toQString(values.front());
        }
    }
    return {};
}

// "3/12" -> 3
int leadingNumber(const QString &value)
{
    static const QRegularExpression re(QStringLiteral(R"(^\s*(\d+))"));
    const QRegularExpressionMatch m = re.match(value);
    return m.hasMatch() ? m.captured(1).toInt() : 0;
}

QByteArray fileName(const QString &path)
{
    return QFile::encodeName(path);
}

// Fill whatever the tags did not provide from the file and folder names.
void applyFallbacks(Track &t)
{
    const QFileInfo info(t.path);
    if (t.title.isEmpty()) {
        QString base = info.completeBaseName();
        static const QRegularExpression numbered(QStringLiteral(R"(^(\d{1,3})[\s._-]+(.+)$)"));
        const QRegularExpressionMatch m = numbered.match(base);
        if (m.hasMatch()) {
            if (t.trackNumber == 0)
                t.trackNumber = m.captured(1).toInt();
            base = m.captured(2);
        }
        const int dash = base.indexOf(QLatin1String(" - "));
        if (dash > 0 && t.artist.isEmpty()) {
            t.artist = base.left(dash).trimmed();
            base = base.mid(dash + 3).trimmed();
        }
        t.title = base;
    }
    if (t.album.isEmpty())
        t.album = info.dir().dirName();
}

QImage imageFromData(const QByteArray &data)
{
    QImage image;
    if (!data.isEmpty())
        image.loadFromData(data);
    return image;
}

#if TAGLIB_MAJOR_VERSION >= 2
// TagLib 2 exposes pictures of every format (including Matroska/WebM from
// 2.2 on) through the generic "PICTURE" complex property.
QImage genericCover(const QString &path)
{
    const TagLib::FileRef ref(fileName(path).constData(), false);
    if (ref.isNull())
        return {};
    QByteArray fallback;
    const TagLib::List<TagLib::VariantMap> pictures = ref.complexProperties("PICTURE");
    for (const TagLib::VariantMap &picture : pictures) {
        const QByteArray data = toQByteArray(picture.value("data").toByteVector());
        if (data.isEmpty())
            continue;
        const QString type = toQString(picture.value("pictureType").toString());
        if (type.isEmpty() || type.compare(QLatin1String("Front Cover"), Qt::CaseInsensitive) == 0) {
            const QImage image = imageFromData(data);
            if (!image.isNull())
                return image;
        }
        if (fallback.isEmpty())
            fallback = data;
    }
    return imageFromData(fallback);
}
#endif

QImage mpegCover(const QString &path)
{
    TagLib::MPEG::File file(fileName(path).constData(), false);
    if (!file.isValid() || !file.hasID3v2Tag())
        return {};
    const TagLib::ID3v2::FrameList frames = file.ID3v2Tag()->frameListMap()["APIC"];
    QByteArray fallback;
    for (TagLib::ID3v2::Frame *frame : frames) {
        auto *pic = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(frame);
        if (!pic)
            continue;
        const QByteArray data = toQByteArray(pic->picture());
        if (pic->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover) {
            const QImage image = imageFromData(data);
            if (!image.isNull())
                return image;
        }
        if (fallback.isEmpty())
            fallback = data;
    }
    return imageFromData(fallback);
}

QImage flacCover(const QString &path)
{
    TagLib::FLAC::File file(fileName(path).constData(), false);
    if (!file.isValid())
        return {};
    QByteArray fallback;
    const TagLib::List<TagLib::FLAC::Picture *> pictures = file.pictureList();
    for (TagLib::FLAC::Picture *pic : pictures) {
        const QByteArray data = toQByteArray(pic->data());
        if (pic->type() == TagLib::FLAC::Picture::FrontCover) {
            const QImage image = imageFromData(data);
            if (!image.isNull())
                return image;
        }
        if (fallback.isEmpty())
            fallback = data;
    }
    return imageFromData(fallback);
}

QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray bytes = file.readAll();
    QStringDecoder utf8(QStringDecoder::Utf8);
    QString text = utf8.decode(bytes);
    if (utf8.hasError())
        text = QString::fromLatin1(bytes); // old LRC files are often Latin-1
    return text;
}

Lyrics mpegSynchronisedLyrics(const QString &path)
{
    TagLib::MPEG::File file(fileName(path).constData(), false);
    if (!file.isValid() || !file.hasID3v2Tag())
        return {};
    const TagLib::ID3v2::FrameList frames = file.ID3v2Tag()->frameListMap()["SYLT"];
    for (TagLib::ID3v2::Frame *frame : frames) {
        auto *sylt = dynamic_cast<TagLib::ID3v2::SynchronizedLyricsFrame *>(frame);
        if (!sylt || sylt->timestampFormat() != TagLib::ID3v2::SynchronizedLyricsFrame::AbsoluteMilliseconds)
            continue;
        QVector<LyricLine> lines;
        for (const auto &entry : sylt->synchedText()) {
            // SYLT entries sometimes carry a leading newline per line.
            QString text = toQString(entry.text);
            lines.push_back({qint64(entry.time), text});
        }
        const Lyrics lyrics = Lyrics::fromLines(lines, true);
        if (!lyrics.isEmpty())
            return lyrics;
    }
    return {};
}

} // namespace

namespace Metadata {

Track readTrack(const QString &path)
{
    Track t;
    t.path = path;

    const TagLib::FileRef ref(fileName(path).constData(), true, TagLib::AudioProperties::Fast);
    if (!ref.isNull()) {
        if (TagLib::Tag *tag = ref.tag()) {
            t.title = toQString(tag->title());
            t.artist = toQString(tag->artist());
            t.album = toQString(tag->album());
            t.genre = toQString(tag->genre());
            t.year = int(tag->year());
            t.trackNumber = int(tag->track());
            t.tagged = !t.title.isEmpty() || !t.artist.isEmpty() || !t.album.isEmpty();
        }
        const TagLib::PropertyMap props = ref.file()->properties();
        t.albumArtist = firstProperty(props, {"ALBUMARTIST", "ALBUM ARTIST", "ALBUM_ARTIST"});
        t.discNumber = leadingNumber(firstProperty(props, {"DISCNUMBER"}));
        if (t.trackNumber == 0)
            t.trackNumber = leadingNumber(firstProperty(props, {"TRACKNUMBER"}));
        if (TagLib::AudioProperties *audio = ref.audioProperties())
            t.durationMs = audio->lengthInMilliseconds();
    }

    applyFallbacks(t);
    return t;
}

QImage readEmbeddedCover(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    QImage image;
    if (suffix == QLatin1String("mp3"))
        image = mpegCover(path);
    else if (suffix == QLatin1String("flac"))
        image = flacCover(path);
#if TAGLIB_MAJOR_VERSION >= 2
    if (image.isNull())
        image = genericCover(path);
#endif
    return image;
}

QImage findFolderCover(const QString &directory)
{
    const QDir dir(directory);
    const QStringList images = dir.entryList(
        {QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.png"),
         QStringLiteral("*.webp"), QStringLiteral("*.bmp")},
        QDir::Files | QDir::Readable, QDir::Name);
    if (images.isEmpty())
        return {};

    // Prefer the conventional names, then anything that looks like a cover.
    static const QStringList preferred{QStringLiteral("cover"), QStringLiteral("folder"),
                                       QStringLiteral("front"), QStringLiteral("album"),
                                       QStringLiteral("albumart")};
    QString best;
    int bestScore = -1;
    for (const QString &name : images) {
        const QString base = QFileInfo(name).completeBaseName().toLower();
        int score = 0;
        for (int i = 0; i < preferred.size(); ++i) {
            if (base == preferred[i]) {
                score = 100 - i;
                break;
            }
            if (base.contains(preferred[i]))
                score = qMax(score, 50 - i);
        }
        if (base.contains(QLatin1String("back")) || base.contains(QLatin1String("cd")))
            score -= 20;
        if (score > bestScore) {
            bestScore = score;
            best = name;
        }
    }
    return QImage(dir.filePath(best));
}

QImage readCover(const QString &path)
{
    QImage image = readEmbeddedCover(path);
    if (image.isNull())
        image = findFolderCover(QFileInfo(path).absolutePath());
    return image;
}

Lyrics readLyrics(const QString &path)
{
    const QFileInfo info(path);
    for (const QString &suffix : {QStringLiteral(".lrc"), QStringLiteral(".LRC")}) {
        const QString sidecar = info.dir().filePath(info.completeBaseName() + suffix);
        if (QFileInfo::exists(sidecar)) {
            const Lyrics lyrics = Lyrics::parse(readTextFile(sidecar));
            if (!lyrics.isEmpty())
                return lyrics;
        }
    }

    if (info.suffix().compare(QLatin1String("mp3"), Qt::CaseInsensitive) == 0) {
        const Lyrics synced = mpegSynchronisedLyrics(path);
        if (!synced.isEmpty())
            return synced;
    }

    const TagLib::FileRef ref(fileName(path).constData(), false);
    if (ref.isNull())
        return {};
    const TagLib::PropertyMap props = ref.file()->properties();
    // USLT frames map to "LYRICS" or "LYRICS:<description>".
    QString text = firstProperty(props, {"LYRICS", "UNSYNCEDLYRICS", "UNSYNCED LYRICS"});
    if (text.isEmpty()) {
        for (auto it = props.begin(); it != props.end(); ++it) {
            if (toQString(it->first).startsWith(QLatin1String("LYRICS")) && !it->second.isEmpty()) {
                text = toQString(it->second.front());
                break;
            }
        }
    }
    return Lyrics::parse(text);
}

} // namespace Metadata
