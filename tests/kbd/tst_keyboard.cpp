#include <QtTest>
#include <QApplication>
#include <QLineEdit>
#include <QVBoxLayout>

#include "AbstractKeyboard.h"
#include "Keyboard.h"
#include "KeyButton.h"
#include "T9Keyboard.h"

using namespace AeaQt;

class TstKeyboard : public QObject
{
    Q_OBJECT
private slots:
    // 现画图标不依赖任何图片插件：每种都得到非空、且确实画了像素的图
    void keyIconsAreDrawn()
    {
        const QList<KeyIcon> kinds = {KeyIcon::Backspace, KeyIcon::CapsLock,
                                      KeyIcon::Enter, KeyIcon::Space, KeyIcon::Hide};
        for (KeyIcon kind : kinds) {
            const QIcon icon = keyIcon(kind);
            QVERIFY(!icon.isNull());
            const QImage img = icon.pixmap(48, 48).toImage();
            int opaque = 0;
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x)
                    if (qAlpha(img.pixel(x, y)) > 128)
                        ++opaque;
            QVERIFY2(opaque > 30, qPrintable(QStringLiteral("kind %1 opaque %2")
                                             .arg(int(kind)).arg(opaque)));
        }
    }

    // 全键盘图标键（大小写/退格/回车/空格）都挂上了图标
    void fullKeyboardIconKeys()
    {
        Keyboard kb;
        kb.setFixedSize(672, 264);
        kb.show();
        QCoreApplication::processEvents();
        const QList<int> iconKeys = {Qt::Key_CapsLock, Qt::Key_Backspace,
                                     Qt::Key_Enter, Qt::Key_Space};
        for (int key : iconKeys) {
            bool found = false;
            for (KeyButton *b : kb.findChildren<KeyButton *>()) {
                if (b->mode().key == key) {
                    found = true;
                    QVERIFY2(!b->icon().isNull(), qPrintable(QString::number(key, 16)));
                }
            }
            QVERIFY(found);
        }
    }

    // 672 宽下「九宫格」「en/汉」按下发的字号放得进可用宽（20 份 / 122 − 18）
    void bottomRowLabelsFit()
    {
        Keyboard kb;
        kb.setFixedSize(672, 264);
        kb.show();
        QCoreApplication::processEvents();
        const int avail = 672 * 20 / 122 - 18;
        int checked = 0;
        for (KeyButton *b : kb.findChildren<KeyButton *>()) {
            const int key = b->mode().key;
            if (key != Qt::Key_Context1 && key != Qt::Key_Context2)
                continue;
            const QRegularExpression rx(QStringLiteral("#\\w+ \\{ font-size: (\\d+)px; font-weight: (\\w+);"));
            const auto m = rx.match(b->styleSheet());
            QVERIFY2(m.hasMatch(), qPrintable(b->styleSheet()));
            QFont f = b->font();
            f.setPixelSize(m.captured(1).toInt());
            f.setBold(m.captured(2) == QLatin1String("bold"));
            const QFontMetrics fm(f);
            const QStringList texts = key == Qt::Key_Context1
                    ? QStringList{QStringLiteral("en/汉"), QStringLiteral("汉/en")}
                    : QStringList{b->text()};
            for (const QString &t : texts)
                QVERIFY2(fm.horizontalAdvance(t) <= avail,
                         qPrintable(QStringLiteral("%1 %2px w=%3 avail=%4")
                                    .arg(t).arg(f.pixelSize())
                                    .arg(fm.horizontalAdvance(t)).arg(avail)));
            ++checked;
        }
        QCOMPARE(checked, 2);
        // 宽度不变时重复 resize 不让样式表无限增长
        const QString before = kb.findChild<KeyButton *>(QStringLiteral("langSwitchKey"))->styleSheet();
        kb.setFixedSize(673, 264);
        kb.setFixedSize(672, 264);
        QCoreApplication::processEvents();
        QCOMPARE(kb.findChild<KeyButton *>(QStringLiteral("langSwitchKey"))->styleSheet(), before);
    }

    // 九宫格英文 + 密码框：按 2 候选栏只给字母直选 a b c 2 A B C，不联想单词
    void t9PasswordShowsLettersOnly()
    {
        QWidget host;
        auto *layout = new QVBoxLayout(&host);
        auto *edit = new QLineEdit(&host);
        edit->setEchoMode(QLineEdit::Password);
        layout->addWidget(edit);
        auto *kb = new T9Keyboard(&host);
        layout->addWidget(kb);
        kb->setEnglishOnly(true);
        kb->setExtraWords({QStringLiteral("ADMIN")});
        host.show();
        QApplication::setActiveWindow(&host);
        edit->setFocus();
        QCoreApplication::processEvents();
        QCOMPARE(QApplication::focusWidget(), edit);

        T9Key *two = nullptr;
        for (T9Key *k : kb->findChildren<T9Key *>())
            if (k->isVisible() && k->text() == QLatin1String("2"))
                two = k;
        QVERIFY(two);
        two->click();
        QCoreApplication::processEvents();

        const QStringList expected = {"a", "b", "c", "A", "B", "C", "2"};
        bool found = false;
        for (T9Strip *strip : kb->findChildren<T9Strip *>()) {
            // 跳过左侧标点竖条（, . ? ! …），只看含字母的候选栏
            if (!strip->isVisible() || !strip->items().contains(QStringLiteral("a")))
                continue;
            QCOMPARE(strip->items(), expected);
            found = true;
        }
        QVERIFY(found);

        // 普通文本框照旧：词联想仍在
        edit->setEchoMode(QLineEdit::Normal);
        auto *plain = new QLineEdit(&host);
        layout->insertWidget(0, plain);
        plain->show();
        plain->setFocus();
        QCoreApplication::processEvents();
        two->click();
        QCoreApplication::processEvents();
        bool hasWord = false;
        for (T9Strip *strip : kb->findChildren<T9Strip *>())
            for (const QString &item : strip->items())
                if (item.size() > 1)
                    hasWord = true;
        QVERIFY(hasWord);
        // 中文引擎在后台线程加载字典：等它结束再析构，避免 "QThread destroyed while running"
        QTest::qWait(3000);
    }
    // @ 不藏深：英文态左侧标点竖条首项是 @，按 1 的字母直选首项也是 @
    void t9AtSignIsReachable()
    {
        QWidget host;
        auto *layout = new QVBoxLayout(&host);
        auto *edit = new QLineEdit(&host);
        layout->addWidget(edit);
        auto *kb = new T9Keyboard(&host);
        layout->addWidget(kb);
        kb->setEnglishOnly(true);
        host.show();
        QApplication::setActiveWindow(&host);
        edit->setFocus();
        QCoreApplication::processEvents();

        T9Strip *punct = nullptr;
        for (T9Strip *strip : kb->findChildren<T9Strip *>())
            if (strip->isVisible() && strip->items().contains(QStringLiteral(";")))
                punct = strip;
        QVERIFY(punct);
        QCOMPARE(punct->items().first(), QStringLiteral("@"));

        // 数字页（123）的标点竖条首项也是 @
        for (T9Key *k : kb->findChildren<T9Key *>())
            if (k->isVisible() && k->text() == QLatin1String("123")) {
                k->click();
                break;
            }
        QCoreApplication::processEvents();
        QVERIFY(punct->items().contains(QStringLiteral("=")));   // 确认已在数字页
        QCOMPARE(punct->items().first(), QStringLiteral("@"));
        for (T9Key *k : kb->findChildren<T9Key *>())
            if (k->isVisible() && k->text() == QString::fromUtf8("英文")) {
                k->click();
                break;
            }
        QCoreApplication::processEvents();

        T9Key *one = nullptr;
        for (T9Key *k : kb->findChildren<T9Key *>())
            if (k->isVisible() && k->text() == QLatin1String("1"))
                one = k;
        QVERIFY(one);
        one->click();
        QCoreApplication::processEvents();
        bool found = false;
        for (T9Strip *strip : kb->findChildren<T9Strip *>()) {
            if (strip == punct || !strip->isVisible() || strip->items().isEmpty())
                continue;
            QCOMPARE(strip->items().first(), QStringLiteral("@"));
            found = true;
        }
        QVERIFY(found);
        QTest::qWait(3000);   // 等中文引擎后台加载结束再析构
    }
};

QTEST_MAIN(TstKeyboard)
#include "tst_keyboard.moc"
