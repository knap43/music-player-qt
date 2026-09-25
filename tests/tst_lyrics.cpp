#include "Lyrics.h"

#include <QtTest>

class TestLyrics : public QObject
{
    Q_OBJECT
private slots:
    void plainText()
    {
        const Lyrics l = Lyrics::parse(QStringLiteral("\n\nFirst line\nSecond line\n\n\n\nThird\n\n"));
        QVERIFY(!l.isSynced());
        QCOMPARE(l.lines().size(), 4); // blank runs collapse to one gap
        QCOMPARE(l.lines().first().text, QStringLiteral("First line"));
        QCOMPARE(l.lines().last().text, QStringLiteral("Third"));
        QCOMPARE(l.lineAt(5000), -1);
    }

    void syncedWithHeadersAndRepeats()
    {
        const Lyrics l = Lyrics::parse(QStringLiteral(
            "[ar:Someone]\n[ti:Song]\n"
            "[00:10.50]Second\n"
            "[00:01.00][01:00.00]Chorus\n"
            "[00:20.123]Third <00:21.00>word\n"));
        QVERIFY(l.isSynced());
        QCOMPARE(l.lines().size(), 4);
        QCOMPARE(l.lines()[0].timeMs, 1000);
        QCOMPARE(l.lines()[0].text, QStringLiteral("Chorus"));
        QCOMPARE(l.lines()[1].timeMs, 10500);
        QCOMPARE(l.lines()[2].timeMs, 20123);
        QCOMPARE(l.lines()[2].text, QStringLiteral("Third word"));
        QCOMPARE(l.lines()[3].timeMs, 60000);

        QCOMPARE(l.lineAt(0), -1);
        QCOMPARE(l.lineAt(1000), 0);
        QCOMPARE(l.lineAt(10499), 0);
        QCOMPARE(l.lineAt(10500), 1);
        QCOMPARE(l.lineAt(999999), 3);
    }

    void offsetShiftsEarlier()
    {
        const Lyrics l = Lyrics::parse(QStringLiteral("[offset:+500]\n[00:02.00]Line\n"));
        QCOMPARE(l.lines().first().timeMs, 1500);
    }

    void bomAndCrLf()
    {
        const Lyrics l = Lyrics::parse(QString(QChar(0xFEFF)) + QStringLiteral("[00:01.0]A\r\n[00:02.0]B\r\n"));
        QVERIFY(l.isSynced());
        QCOMPARE(l.lines().size(), 2);
        QCOMPARE(l.lines()[0].timeMs, 1000);
    }

    void emptyInput()
    {
        QVERIFY(Lyrics::parse(QString()).isEmpty());
        QVERIFY(Lyrics::parse(QStringLiteral("\n  \n[ar:Only a header]\n")).isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestLyrics)
#include "tst_lyrics.moc"
