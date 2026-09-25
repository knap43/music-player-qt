#include "Lyrics.h"

#include <QRegularExpression>

#include <algorithm>

namespace {

qint64 toMs(const QString &minutes, const QString &seconds, const QString &fraction)
{
    qint64 ms = minutes.toLongLong() * 60000 + seconds.toLongLong() * 1000;
    if (!fraction.isEmpty())
        ms += fraction.left(3).leftJustified(3, QLatin1Char('0')).toLongLong();
    return ms;
}

void trimBlankEdges(QVector<LyricLine> &lines)
{
    while (!lines.isEmpty() && lines.first().text.trimmed().isEmpty())
        lines.removeFirst();
    while (!lines.isEmpty() && lines.last().text.trimmed().isEmpty())
        lines.removeLast();
}

} // namespace

Lyrics Lyrics::parse(const QString &input)
{
    static const QRegularExpression timeTag(
        QStringLiteral(R"(^\s*\[(\d{1,3}):(\d{1,2})(?:[.:](\d{1,3}))?\])"));
    static const QRegularExpression wordTag(
        QStringLiteral(R"(<\d{1,3}:\d{1,2}(?:[.:]\d{1,3})?>)"));
    static const QRegularExpression infoTag(QStringLiteral(R"(^\s*\[([A-Za-z#]+):(.*)\]\s*$)"));

    QString text = input;
    if (text.startsWith(QChar(0xFEFF)))
        text.remove(0, 1);

    const QStringList rawLines = text.split(QRegularExpression(QStringLiteral("\r\n|\r|\n")));

    QVector<LyricLine> synced;
    QVector<LyricLine> plain;
    qint64 offsetMs = 0;

    for (const QString &raw : rawLines) {
        QString rest = raw;
        QVector<qint64> times;
        for (;;) {
            const QRegularExpressionMatch m = timeTag.match(rest);
            if (!m.hasMatch())
                break;
            times << toMs(m.captured(1), m.captured(2), m.captured(3));
            rest = rest.mid(m.capturedEnd());
        }

        if (!times.isEmpty()) {
            rest.remove(wordTag);
            rest = rest.trimmed();
            for (qint64 t : times)
                synced.push_back({t, rest});
            continue;
        }

        const QRegularExpressionMatch info = infoTag.match(raw);
        if (info.hasMatch()) {
            // LRC header such as [ar:Artist] or [offset:+250]; never lyrics.
            if (info.captured(1).compare(QLatin1String("offset"), Qt::CaseInsensitive) == 0)
                offsetMs = info.captured(2).trimmed().toLongLong();
            continue;
        }

        QString line = raw;
        while (line.endsWith(QLatin1Char(' ')) || line.endsWith(QLatin1Char('\t')))
            line.chop(1);
        plain.push_back({-1, line});
    }

    if (!synced.isEmpty()) {
        // A positive offset makes the lyrics appear earlier.
        for (LyricLine &line : synced)
            line.timeMs = qMax<qint64>(0, line.timeMs - offsetMs);
        std::stable_sort(synced.begin(), synced.end(),
                         [](const LyricLine &a, const LyricLine &b) { return a.timeMs < b.timeMs; });
        return fromLines(synced, true);
    }

    return fromLines(plain, false);
}

Lyrics Lyrics::fromLines(QVector<LyricLine> lines, bool synced)
{
    if (!synced)
        trimBlankEdges(lines);
    Lyrics lyrics;
    // Collapse runs of blank lines so paragraphs keep a single gap.
    for (const LyricLine &line : lines) {
        const bool blank = line.text.trimmed().isEmpty();
        if (!synced && blank && !lyrics.m_lines.isEmpty()
            && lyrics.m_lines.last().text.trimmed().isEmpty())
            continue;
        lyrics.m_lines.push_back(line);
    }
    lyrics.m_synced = synced && !lyrics.m_lines.isEmpty();

    bool anyText = false;
    for (const LyricLine &line : lyrics.m_lines)
        anyText = anyText || !line.text.trimmed().isEmpty();
    if (!anyText)
        return Lyrics();
    return lyrics;
}

int Lyrics::lineAt(qint64 positionMs) const
{
    if (!m_synced || m_lines.isEmpty())
        return -1;
    const auto it = std::upper_bound(
        m_lines.cbegin(), m_lines.cend(), positionMs,
        [](qint64 pos, const LyricLine &line) { return pos < line.timeMs; });
    return int(it - m_lines.cbegin()) - 1;
}
