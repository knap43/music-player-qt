#include "PlayQueue.h"

#include <QSet>
#include <QtTest>

namespace {

QVector<Track> makeTracks(int n, const QString &prefix = QStringLiteral("t"))
{
    QVector<Track> tracks;
    for (int i = 0; i < n; ++i) {
        Track t;
        t.path = QStringLiteral("/music/%1%2.flac").arg(prefix).arg(i);
        t.title = prefix + QString::number(i);
        tracks << t;
    }
    return tracks;
}

QString currentTitle(const PlayQueue &q)
{
    return q.currentTrack() ? q.currentTrack()->title : QString();
}

} // namespace

class TestPlayQueue : public QObject
{
    Q_OBJECT
private slots:
    void linearAdvanceAndBack()
    {
        PlayQueue q;
        q.append(makeTracks(3));
        QCOMPARE(q.advance(false), 0);
        QCOMPARE(q.advance(false), 1);
        QCOMPARE(q.advance(false), 2);
        QCOMPARE(q.advance(false), -1); // end of queue, repeat off
        QCOMPARE(q.goBack(), 1);
        QCOMPARE(q.goBack(), 0);
        QCOMPARE(q.goBack(), -1);
    }

    void repeatModes()
    {
        PlayQueue q;
        q.append(makeTracks(2));
        q.setRepeat(PlayQueue::Repeat::All);
        q.setCurrentRow(1);
        QCOMPARE(q.advance(false), 0); // wraps
        q.setRepeat(PlayQueue::Repeat::One);
        QCOMPARE(q.advance(true), 0);  // finished track replays
        QCOMPARE(q.advance(false), 1); // but "next" still moves on
    }

    void removingCurrentContinuesWithFollower()
    {
        PlayQueue q;
        q.append(makeTracks(4));
        q.setCurrentRow(1);
        q.removeRowsList({1});
        QCOMPARE(q.currentRow(), -1);
        QCOMPARE(q.advance(false), 1);
        QCOMPARE(currentTitle(q), QStringLiteral("t2"));
    }

    void insertAfterCurrentPlaysNext()
    {
        PlayQueue q;
        q.append(makeTracks(3));
        q.setCurrentRow(0);
        const int first = q.insertAfterCurrent(makeTracks(2, QStringLiteral("n")));
        QCOMPARE(first, 1);
        q.advance(false);
        QCOMPARE(currentTitle(q), QStringLiteral("n0"));
        q.advance(false);
        QCOMPARE(currentTitle(q), QStringLiteral("n1"));
        q.advance(false);
        QCOMPARE(currentTitle(q), QStringLiteral("t1"));
    }

    void moveKeepsCurrent()
    {
        PlayQueue q;
        q.append(makeTracks(5));
        q.setCurrentRow(2);
        QCOMPARE(q.moveRowsTo({3, 4}, 0), 0);
        QCOMPARE(q.trackAt(0).title, QStringLiteral("t3"));
        QCOMPARE(q.trackAt(1).title, QStringLiteral("t4"));
        QCOMPARE(currentTitle(q), QStringLiteral("t2"));
        QCOMPARE(q.currentRow(), 4);
    }

    void shufflePlaysEverythingOnce()
    {
        PlayQueue q;
        q.append(makeTracks(20));
        q.setShuffle(true);
        QSet<QString> seen;
        for (int i = 0; i < 20; ++i) {
            QVERIFY(q.advance(false) >= 0);
            seen.insert(currentTitle(q));
        }
        QCOMPARE(seen.size(), 20);
        QCOMPARE(q.advance(false), -1);
    }

    void shuffleBackRetracesHistory()
    {
        PlayQueue q;
        q.append(makeTracks(10));
        q.setShuffle(true);
        QStringList history;
        for (int i = 0; i < 5; ++i) {
            q.advance(false);
            history << currentTitle(q);
        }
        for (int i = 3; i >= 0; --i) {
            q.goBack();
            QCOMPARE(currentTitle(q), history[i]);
        }
    }

    void shuffleIncludesTracksAddedLater()
    {
        PlayQueue q;
        q.append(makeTracks(5));
        q.setShuffle(true);
        q.advance(false);
        q.advance(false);
        q.append(makeTracks(5, QStringLiteral("late")));
        QSet<QString> seen;
        while (q.advance(false) >= 0)
            seen.insert(currentTitle(q));
        QCOMPARE(seen.size(), 8); // 3 unplayed originals + 5 new ones
        for (int i = 0; i < 5; ++i)
            QVERIFY(seen.contains(QStringLiteral("late%1").arg(i)));
    }

    void shuffleManualPickContinuesFromThere()
    {
        PlayQueue q;
        q.append(makeTracks(6));
        q.setShuffle(true);
        q.advance(false);
        const QString before = currentTitle(q);
        q.setCurrentRow(5);
        QCOMPARE(currentTitle(q), QStringLiteral("t5"));
        q.goBack();
        QCOMPARE(currentTitle(q), before);
    }

    void shuffleRepeatAllReshuffles()
    {
        PlayQueue q;
        q.append(makeTracks(4));
        q.setShuffle(true);
        q.setRepeat(PlayQueue::Repeat::All);
        for (int i = 0; i < 4; ++i)
            q.advance(false);
        const QString last = currentTitle(q);
        QVERIFY(q.advance(false) >= 0);
        QVERIFY(currentTitle(q) != last); // never the same track twice in a row
    }
};

QTEST_APPLESS_MAIN(TestPlayQueue)
#include "tst_playqueue.moc"
