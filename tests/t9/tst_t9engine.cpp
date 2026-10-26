// T9 拼音引擎单测（源自 aerocount docs/17 7.1 用例清单）
#include <QtTest>

#include "T9PinyinEngine.h"

using namespace AeaQt;

class TstT9Engine : public QObject
{
    Q_OBJECT

private:
    static bool hasWord(const QList<T9Candidate> &list, const QString &word)
    {
        for (const T9Candidate &c : list)
            if (c.word == word)
                return true;
        return false;
    }

private slots:
    void letterToDigit()
    {
        QCOMPARE(T9PinyinEngine::letterToDigit(QLatin1Char('a')), '2');
        QCOMPARE(T9PinyinEngine::letterToDigit(QLatin1Char('c')), '2');
        QCOMPARE(T9PinyinEngine::letterToDigit(QLatin1Char('s')), '7');
        QCOMPARE(T9PinyinEngine::letterToDigit(QLatin1Char('v')), '8');
        QCOMPARE(T9PinyinEngine::letterToDigit(QLatin1Char('Z')), '9');
        QCOMPARE(T9PinyinEngine::letterToDigit(QLatin1Char('1')), char(0));
        QCOMPARE(T9PinyinEngine::pinyinToDigits(QStringLiteral("zhong guo")),
                 QByteArray("94664486"));
    }

    void consumeKeepsRest()
    {
        QCOMPARE(T9PinyinEngine::consume("94664'486", 5), QByteArray("486"));
        QCOMPARE(T9PinyinEngine::consume("64", 2), QByteArray());
        QCOMPARE(T9PinyinEngine::stripSeparators("94'664"), QByteArray("94664"));
    }

    void emptyEngineNoCrash()
    {
        T9PinyinEngine engine;
        QVERIFY(!engine.isReady());
        QVERIFY(engine.candidates("64").isEmpty());
        QVERIFY(engine.candidates("").isEmpty());
        QVERIFY(engine.compositions("").isEmpty());
    }

    void compositions94664()
    {
        T9PinyinEngine engine;
        const QStringList list = engine.compositions("94664");
        QVERIFY2(list.contains(QStringLiteral("zhong")), qPrintable(list.join(',')));
        QVERIFY2(list.contains(QStringLiteral("xin")), qPrintable(list.join(',')));
        QVERIFY(list.size() <= 12);
    }

    void separatorForcesSplit()
    {
        T9PinyinEngine engine;
        const QStringList list = engine.compositions("94'664");
        QVERIFY(!list.isEmpty());
        QVERIFY2(list.first().contains(QLatin1Char('\'')), qPrintable(list.first()));
        QVERIFY(!list.first().startsWith(QStringLiteral("zhong")));
    }

    void candidates64()
    {
        T9PinyinEngine engine;
        engine.loadSync();
        QVERIFY(engine.isReady());
        const QList<T9Candidate> list = engine.candidates("64");
        QVERIFY(hasWord(list, QStringLiteral("你")));
        QVERIFY(list.size() <= 60);
        // 前缀匹配（您=nin=646）：K4 下 64 的完整匹配单字（ni/mi…）与简拼完整
        // 匹配的两字词（数千条）都排在前缀匹配之前，「您」在 64 下实际排不进候选；
        // 多输一位成完整匹配后升到首位。原用例断言 64 即含「您」，与 K4 排序
        // 自相矛盾（aerocount 分叉线该单测只编译未运行，未暴露；2026-10-26 修正）
        const QList<T9Candidate> nin = engine.candidates("646");
        QVERIFY(!nin.isEmpty());
        QCOMPARE(nin.first().word, QStringLiteral("您"));
    }

    void lockedPinyinFilters()
    {
        T9PinyinEngine engine;
        engine.loadSync();
        // 64 既是 ni 也是 mi；锁定 mi 后不应再出现"你"
        const QList<T9Candidate> list = engine.candidates("64", QStringLiteral("mi"));
        QVERIFY(!list.isEmpty());
        QVERIFY(!hasWord(list, QStringLiteral("你")));
    }

    void partialWordConsumesPrefix()
    {
        T9PinyinEngine engine;
        engine.loadSync();
        const QList<T9Candidate> list = engine.candidates("94664486");
        QVERIFY(hasWord(list, QStringLiteral("中国")));
        for (const T9Candidate &c : list) {
            if (c.word == QStringLiteral("中"))
                QCOMPARE(c.digitCount, 5);
        }
    }
};

QTEST_GUILESS_MAIN(TstT9Engine)
#include "tst_t9engine.moc"
