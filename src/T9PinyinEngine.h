// Keyboard/T9PinyinEngine.h
// 九宫格（T9）拼音引擎：纯逻辑，不含 QWidget。数字序列 -> 拼音组合 / 汉字候选。
// 词库复用模板内置三字典（resources/dicts/，只读）。后台 QThread 加载，
// 完成后回到引擎线程一次性替换（docs/17 4.3/K3）。
// 加载走扁平二进制缓存（2026-11-13）：首启解析文本字典并原子写缓存到
// AppDataLocation，之后启动只 mmap+校验（几十 ms，替代原 ~5.5s 全量解析）。
// 缓存指纹 = 宿主 exe 的 size+mtime + 格式版本；失效/损坏自动回退文本解析。
#ifndef AEA_T9_PINYIN_ENGINE_H
#define AEA_T9_PINYIN_ENGINE_H

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QStringList>

namespace AeaQt {

// 候选：上屏文本 + 该词占用的输入数字位数（不含分词符）
struct T9Candidate
{
    QString word;
    int digitCount = 0;
};

// 引擎全部索引（后台线程构建，完成后只读）。扁平表示：字符串池 + 定长记录 +
// 前缀桶索引，文本解析与 mmap 缓存共用同一查询路径。定义在 .cpp。
struct T9PinyinData;

class T9PinyinEngine : public QObject
{
    Q_OBJECT
public:
    explicit T9PinyinEngine(QObject *parent = nullptr);
    ~T9PinyinEngine() override;

    static char letterToDigit(QChar c);                   // abc=2..wxyz=9，非字母返回 0
    static QByteArray pinyinToDigits(const QString &py);  // 非字母字符跳过
    static QByteArray stripSeparators(const QByteArray &input);
    // 从输入开头扣掉 digitCount 位数字（连带其间与紧随其后的分词符）
    static QByteArray consume(const QByteArray &input, int digitCount);
    static const QStringList &syllables();                // 内置全部合法音节

    void loadAsync();   // 后台加载，完成发 ready()；重复调用无副作用
    void loadSync();    // 同步加载（单测用）
    bool isReady() const { return !m_data.isNull(); }
    bool isLoading() const { return m_loading; }

    // 拼音组合栏（<= 12 个）：首项整串最长切分（音节以 ' 连接），
    // 其后首音节的其他可能，再后首位数字对应的字母
    QStringList compositions(const QByteArray &input) const;

    // 候选：input 可含分词符；lockedPinyin 非空时要求拼音（去空格）以它开头；已去重
    QList<T9Candidate> candidates(const QByteArray &input,
                                  const QString &lockedPinyin = QString(),
                                  int limit = 60) const;

signals:
    void ready();

private:
    static QSharedPointer<const T9PinyinData> buildData();
    // 复用进程级共享词库（见 .cpp 匿名命名空间说明）：已有数据时直接采纳并
    // emit ready()，返回 true；没有则返回 false，调用方自己决定起不起加载。
    bool adoptSharedData();

    QSharedPointer<const T9PinyinData> m_data;
    bool m_loading = false;
};

}

#endif // AEA_T9_PINYIN_ENGINE_H