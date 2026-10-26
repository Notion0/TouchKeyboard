#include "T9PinyinEngine.h"

#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QThread>
#include <QDebug>

#include <algorithm>
#include <functional>

#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
#include <QStringConverter>
#endif

// 三字典路径宏与打开兜底（扁平/嵌套双路径）与 26 键 Keyboard.cpp 共用一处
#include "TouchKbdDict.h"

using namespace AeaQt;

namespace {

const char kSeparator = '\'';
const int kMaxSyllableLen = 6;   // 最长音节 zhuang / chuang / shuang

// 音节索引：数字串 -> 音节（按单字表出现顺序），以及全部音节数字串的前缀集合
struct SyllableIndex
{
    QStringList all;
    QHash<QByteArray, QStringList> byDigits;
    QSet<QByteArray> prefixes;
};

const SyllableIndex &syllableIndex()
{
    // C++11 局部静态：首次调用时构建，线程安全（后台加载线程也会用到）
    static const SyllableIndex index = [] {
        SyllableIndex idx;
        QFile file;
        if (!openTouchKbdDict(file, QStringLiteral(TOUCHKBD_PINYIN_DICT))) {
            qWarning() << "T9: 单字表打开失败，音节表为空：" << TOUCHKBD_PINYIN_DICT;
            return idx;
        }
        QSet<QString> seen;
        // 单字表每行「汉字 + 拼音」，拼音即行内首个 ASCII 字母起的部分
        while (!file.atEnd()) {
            const QString line = QString::fromUtf8(file.readLine()).trimmed();
            int i = 0;
            while (i < line.size() && line.at(i).unicode() >= 0x80)
                ++i;
            const QString py = line.mid(i).trimmed().toLower();
            if (py.isEmpty() || py.size() > kMaxSyllableLen || seen.contains(py))
                continue;
            bool ok = true;
            for (QChar c : py)
                ok = ok && c >= QLatin1Char('a') && c <= QLatin1Char('z');
            if (!ok)
                continue;
            seen.insert(py);
            idx.all.append(py);
            const QByteArray d = T9PinyinEngine::pinyinToDigits(py);
            idx.byDigits[d].append(py);
            for (int n = 1; n <= d.size(); ++n)
                idx.prefixes.insert(d.left(n));
        }
        return idx;
    }();
    return index;
}

} // namespace

T9PinyinEngine::T9PinyinEngine(QObject *parent)
    : QObject(parent)
{
}

T9PinyinEngine::~T9PinyinEngine() = default;

char T9PinyinEngine::letterToDigit(QChar c)
{
    static const char map[26] = {
        '2','2','2', '3','3','3', '4','4','4', '5','5','5', '6','6','6',
        '7','7','7','7', '8','8','8', '9','9','9','9'
    };
    const ushort u = c.toLower().unicode();
    if (u >= 'a' && u <= 'z')
        return map[u - 'a'];
    if (u == 0x00FC)   // ü 与 v 同键
        return '8';
    return 0;
}

QByteArray T9PinyinEngine::pinyinToDigits(const QString &py)
{
    QByteArray out;
    out.reserve(py.size());
    for (QChar c : py) {
        const char d = letterToDigit(c);
        if (d)
            out.append(d);
    }
    return out;
}

QByteArray T9PinyinEngine::stripSeparators(const QByteArray &input)
{
    QByteArray out = input;
    out.replace(kSeparator, QByteArray());
    return out;
}

QByteArray T9PinyinEngine::consume(const QByteArray &input, int digitCount)
{
    int i = 0;
    int taken = 0;
    while (i < input.size() && taken < digitCount) {
        if (input.at(i) != kSeparator)
            ++taken;
        ++i;
    }
    while (i < input.size() && input.at(i) == kSeparator)
        ++i;
    return input.mid(i);
}

const QStringList &T9PinyinEngine::syllables()
{
    return syllableIndex().all;
}

namespace {

void addEntry(T9PinyinData &data, const QString &word, const QString &pinyin,
              float weight, quint8 source)
{
    const QString py = pinyin.simplified().toLower();
    if (word.isEmpty() || py.isEmpty())
        return;

    T9Entry e;
    e.word = word;
    e.pinyin = py;
    e.digits = T9PinyinEngine::pinyinToDigits(py);
    if (e.digits.isEmpty())
        return;
    if (py.contains(QLatin1Char(' '))) {
        for (const QString &s : py.split(QLatin1Char(' '), Qt::SkipEmptyParts))
            e.abbr.append(T9PinyinEngine::letterToDigit(s.at(0)));
    }
    e.weight = weight;
    e.source = source;

    const int index = data.entries.size();
    data.byPrefix2[e.digits.left(2)].append(index);
    if (!e.abbr.isEmpty())
        data.abbrPrefix2[e.abbr.left(2)].append(index);
    data.entries.append(e);
}

void loadGoogle(T9PinyinData &data)
{
    QFile file;
    if (!openTouchKbdDict(file, QStringLiteral(TOUCHKBD_GOOGLE_DICT))) {
        qWarning() << "T9: 谷歌字典打开失败：" << TOUCHKBD_GOOGLE_DICT;
        return;
    }
    QTextStream in(&file);
#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
    in.setEncoding(QStringConverter::Encoding::Utf16);   // BOM 自适应（与 Keyboard.cpp 同口径）
#else
    in.setCodec("UTF-16");   // Qt5 等价 API
#endif
    // 行格式「汉字 词频 标志 拼 音…」：前三段按空白切，其余为拼音
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() < 4)
            continue;
        bool ok = false;
        const float weight = parts.at(1).toFloat(&ok);
        if (!ok)
            continue;
        addEntry(data, parts.at(0), parts.mid(3).join(QLatin1Char(' ')), weight, 0);
    }
}

void loadSingle(T9PinyinData &data)
{
    QFile file;
    if (!openTouchKbdDict(file, QStringLiteral(TOUCHKBD_PINYIN_DICT))) {
        qWarning() << "T9: 单字表打开失败：" << TOUCHKBD_PINYIN_DICT;
        return;
    }
    static const QRegularExpression hanzi(QStringLiteral("^[\\x{4E00}-\\x{9FA5}]+"));
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        const QRegularExpressionMatch m = hanzi.match(line);
        if (!m.hasMatch())
            continue;
        addEntry(data, m.captured(0), line.mid(m.capturedLength()), 0, 1);
    }
}

void loadPhrase(T9PinyinData &data)
{
    QFile file;
    if (!openTouchKbdDict(file, QStringLiteral(TOUCHKBD_PINYIN_PHRASE_DICT))) {
        qWarning() << "T9: 词组表打开失败：" << TOUCHKBD_PINYIN_PHRASE_DICT;
        return;
    }
    // 行格式「词组: 拼 音」；与 Keyboard.cpp 同样用 indexOf 切分，不逐行跑正则
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon <= 0)
            continue;
        addEntry(data, line.left(colon), line.mid(colon + 1), 0, 2);
    }
}

} // namespace

QSharedPointer<const T9PinyinData> T9PinyinEngine::buildData()
{
    QSharedPointer<T9PinyinData> data(new T9PinyinData);
    data->entries.reserve(500000);
    syllableIndex();   // 顺带在后台把音节表建好，避免首次按键时在 GUI 线程读文件
    loadGoogle(*data);
    loadSingle(*data);
    loadPhrase(*data);
    if (data->entries.isEmpty())
        qWarning() << "T9: 三个字典全部为空，中文候选不可用（检查 qrc 资源路径/alias）";
    return data;
}

void T9PinyinEngine::loadSync()
{
    m_data = buildData();
    m_loading = false;
    emit ready();
}

namespace {

// 不用 QThread::create：Qt5 里它经 std::future 实现，sysroot 的 GCC 12 头文件会把
// std::exception_ptr::_M_release（CXXABI_1.3.13）内联进产物，而板子 Buildroot 的
// libstdc++ 是 GCC 10.3，启动即报 "CXXABI_1.3.13 not found"（2026-10-22 实测）
class T9LoadThread : public QThread
{
public:
    T9LoadThread(T9PinyinEngine *engine,
                 std::function<void(QSharedPointer<const T9PinyinData>)> done)
        : QThread(engine), m_done(std::move(done)) {}

    QSharedPointer<const T9PinyinData> (*build)() = nullptr;

protected:
    void run() override { m_done(build()); }

private:
    std::function<void(QSharedPointer<const T9PinyinData>)> m_done;
};

} // namespace

void T9PinyinEngine::loadAsync()
{
    if (m_loading || isReady())
        return;
    m_loading = true;

    // 后台线程只构建局部对象，经 queued 调用回到本对象所在线程再一次性替换（T8）
    auto *thread = new T9LoadThread(this, [this](QSharedPointer<const T9PinyinData> data) {
        QMetaObject::invokeMethod(this, [this, data] {
            m_data = data;
            m_loading = false;
            emit ready();
        }, Qt::QueuedConnection);
    });
    thread->build = &T9PinyinEngine::buildData;
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start(QThread::LowPriority);
}

namespace {

// 从 pos 起把 s 切成音节（遇分词符强制断开），成功时 out 依次追加每个音节的写法。
// 优先最长音节，失败回溯；末尾不完整音节按"某音节数字串以它开头"判合法
bool splitSyllables(const QByteArray &s, int pos, QStringList &out)
{
    while (pos < s.size() && s.at(pos) == kSeparator)
        ++pos;
    if (pos >= s.size())
        return true;

    int segEnd = s.indexOf(kSeparator, pos);
    if (segEnd < 0)
        segEnd = s.size();

    const SyllableIndex &idx = syllableIndex();
    for (int len = qMin(kMaxSyllableLen, segEnd - pos); len >= 1; --len) {
        const QByteArray chunk = s.mid(pos, len);
        auto it = idx.byDigits.constFind(chunk);
        if (it != idx.byDigits.constEnd()) {
            out.append(it->first());
            if (splitSyllables(s, pos + len, out))
                return true;
            out.removeLast();
        }
        if (pos + len == segEnd && idx.prefixes.contains(chunk)) {
            // 不完整音节：取第一个以此数字串开头的音节的前 len 个字母
            for (const QString &syl : idx.all) {
                if (T9PinyinEngine::pinyinToDigits(syl).startsWith(chunk)) {
                    out.append(syl.left(len));
                    break;
                }
            }
            if (splitSyllables(s, pos + len, out))
                return true;
            out.removeLast();
        }
    }
    return false;
}

QString lettersOf(char digit)
{
    static const char *const table[10] = {
        "", "", "abc", "def", "ghi", "jkl", "mno", "pqrs", "tuv", "wxyz"
    };
    if (digit < '0' || digit > '9')
        return QString();
    return QString::fromLatin1(table[digit - '0']);
}

} // namespace

QStringList T9PinyinEngine::compositions(const QByteArray &input) const
{
    QStringList result;
    const QByteArray s = input.left(64);
    if (stripSeparators(s).isEmpty())
        return result;

    auto add = [&result](const QString &text) {
        if (!text.isEmpty() && !result.contains(text) && result.size() < 12)
            result.append(text);
    };

    QStringList parts;
    if (splitSyllables(s, 0, parts))
        add(parts.join(QLatin1Char(kSeparator)));

    // 首音节的其他可能（同一首段内，由长到短）
    int start = 0;
    while (start < s.size() && s.at(start) == kSeparator)
        ++start;
    int segEnd = s.indexOf(kSeparator, start);
    if (segEnd < 0)
        segEnd = s.size();
    const SyllableIndex &idx = syllableIndex();
    for (int len = qMin(kMaxSyllableLen, segEnd - start); len >= 1; --len) {
        auto it = idx.byDigits.constFind(s.mid(start, len));
        if (it == idx.byDigits.constEnd())
            continue;
        for (const QString &syl : *it)
            add(syl);
    }

    // 首位数字对应的字母（简拼用）
    if (start < s.size()) {
        for (QChar c : lettersOf(s.at(start)))
            add(QString(c));
    }
    return result;
}

namespace {

struct Hit
{
    int index;
    quint8 category;  // 0 全拼完整 / 1 简拼完整 / 2 全拼前缀 / 3 简拼前缀 / 4 词只占输入前段
    int consumed;     // 占用的输入数字位数
};

// 分词符位置（去分词符后的数字下标）都必须落在该词的音节边界上
bool separatorsAligned(const QString &pinyin, const QList<int> &seps, int limit)
{
    if (seps.isEmpty())
        return true;
    QList<int> bounds;
    int acc = 0;
    for (const QString &syl : pinyin.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        acc += syl.size();
        bounds.append(acc);
    }
    for (int p : seps) {
        if (p >= limit)
            break;
        if (!bounds.contains(p))
            return false;
    }
    return true;
}

} // namespace

QList<T9Candidate> T9PinyinEngine::candidates(const QByteArray &input,
                                              const QString &lockedPinyin,
                                              int limit) const
{
    QList<T9Candidate> result;
    if (!m_data || limit <= 0)
        return result;

    const QByteArray d = stripSeparators(input);
    if (d.isEmpty())
        return result;

    QList<int> seps;   // 分词符在去分词符串里的位置
    int digitIndex = 0;
    for (char c : input) {
        if (c == kSeparator)
            seps.append(digitIndex);
        else
            ++digitIndex;
    }
    const QString locked = lockedPinyin.toLower().remove(QLatin1Char('\''));

    const T9PinyinData &data = *m_data;
    QList<Hit> hits;

    auto lockedOk = [&locked](const T9Entry &e) {
        return locked.isEmpty() || QString(e.pinyin).remove(QLatin1Char(' ')).startsWith(locked);
    };

    auto scanFull = [&](const QList<int> &bucket) {
        for (int i : bucket) {
            const T9Entry &e = data.entries.at(i);
            quint8 cat;
            int consumed;
            if (e.digits == d) {
                cat = 0; consumed = d.size();
            } else if (e.digits.startsWith(d)) {
                // 只输一位时只出单字，免得几万条词组参与排序
                if (d.size() == 1 && e.pinyin.contains(QLatin1Char(' ')))
                    continue;
                cat = 2; consumed = d.size();
            } else if (d.startsWith(e.digits)) {
                cat = 4; consumed = e.digits.size();
            } else {
                continue;
            }
            if (!separatorsAligned(e.pinyin, seps, consumed) || !lockedOk(e))
                continue;
            hits.append({i, cat, consumed});
        }
    };

    auto scanAbbr = [&](const QList<int> &bucket) {
        for (int i : bucket) {
            const T9Entry &e = data.entries.at(i);
            quint8 cat;
            if (e.abbr == d)
                cat = 1;
            else if (e.abbr.startsWith(d))
                cat = 3;
            else
                continue;
            if (!lockedOk(e))
                continue;
            hits.append({i, cat, int(d.size())});
        }
    };

    if (d.size() == 1) {
        scanFull(data.byPrefix2.value(d));
        for (char c = '2'; c <= '9'; ++c)
            scanFull(data.byPrefix2.value(d + c));
    } else {
        scanFull(data.byPrefix2.value(d.left(2)));
        scanFull(data.byPrefix2.value(d.left(1)));   // 单字母音节（a/e/o…）
        scanAbbr(data.abbrPrefix2.value(d.left(2)));
    }

    // K4：类别 > 来源（谷歌/单字/词组）> 词频 > 文件顺序；"只占前段"的词占得越多越靠前
    auto less = [&data](const Hit &a, const Hit &b) {
        if (a.category != b.category)
            return a.category < b.category;
        if (a.consumed != b.consumed)
            return a.consumed > b.consumed;
        const T9Entry &ea = data.entries.at(a.index);
        const T9Entry &eb = data.entries.at(b.index);
        if (ea.source != eb.source)
            return ea.source < eb.source;
        if (ea.weight != eb.weight)
            return ea.weight > eb.weight;
        return a.index < b.index;
    };
    // 同一个词会从多张表命中，先多排一些再去重
    const int sortCount = qMin(hits.size(), limit * 8);
    std::partial_sort(hits.begin(), hits.begin() + sortCount, hits.end(), less);

    QSet<QString> seen;
    for (int k = 0; k < sortCount && result.size() < limit; ++k) {
        const T9Entry &e = data.entries.at(hits.at(k).index);
        if (seen.contains(e.word))
            continue;
        seen.insert(e.word);
        // 显式逐字段构造：T9Candidate 带默认成员初始值，C++11 下不是聚合，
        // 不能 {word, n} 列表初始化（模板标称 C++11，Qt5 线按 c++11 编）
        T9Candidate c;
        c.word = e.word;
        c.digitCount = hits.at(k).consumed;
        result.append(c);
    }
    return result;
}
