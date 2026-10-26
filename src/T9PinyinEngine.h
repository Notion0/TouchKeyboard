// Keyboard/T9PinyinEngine.h
// 九宫格（T9）拼音引擎：纯逻辑，不含 QWidget。数字序列 -> 拼音组合 / 汉字候选。
// 词库复用模板内置三字典（resources/dicts/，只读）。后台 QThread 加载，
// 完成后回到引擎线程一次性替换（docs/17 4.3/K3）。
#ifndef AEA_T9_PINYIN_ENGINE_H
#define AEA_T9_PINYIN_ENGINE_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QStringList>

namespace AeaQt {

// 词库条目。pinyin 以空格分音节（"zhong guo"）；digits 为全拼数字串；
// abbr 为多音节词首字母数字串（单音节为空）
struct T9Entry
{
    QString word;
    QString pinyin;
    QByteArray digits;
    QByteArray abbr;
    float weight = 0;     // 谷歌表词频；其余来源为 0
    quint8 source = 0;    // 0 谷歌表 / 1 单字表 / 2 词组表（K4 排序次序）
};

// 候选：上屏文本 + 该词占用的输入数字位数（不含分词符）
struct T9Candidate
{
    QString word;
    int digitCount = 0;
};

// 引擎全部索引（后台线程构建，完成后只读）
struct T9PinyinData
{
    QList<T9Entry> entries;
    QHash<QByteArray, QList<int>> byPrefix2;   // digits 前 2 位 -> 下标
    QHash<QByteArray, QList<int>> abbrPrefix2; // abbr 前 2 位 -> 下标
};

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

    QSharedPointer<const T9PinyinData> m_data;
    bool m_loading = false;
};

}

#endif // AEA_T9_PINYIN_ENGINE_H
