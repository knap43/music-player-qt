#pragma once

#include <QString>
#include <QVector>

struct LyricLine {
    qint64 timeMs = -1; // -1 for unsynchronised lyrics
    QString text;
};

// Plain or LRC-style time-synchronised lyrics.
class Lyrics
{
public:
    static Lyrics parse(const QString &text);
    static Lyrics fromLines(QVector<LyricLine> lines, bool synced);

    bool isEmpty() const { return m_lines.isEmpty(); }
    bool isSynced() const { return m_synced; }
    const QVector<LyricLine> &lines() const { return m_lines; }

    // Index of the line being sung at the given position, or -1 before the
    // first timestamp (and always -1 for unsynchronised lyrics).
    int lineAt(qint64 positionMs) const;

private:
    QVector<LyricLine> m_lines;
    bool m_synced = false;
};
