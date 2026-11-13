#include "T9PinyinEngine.h"

#include <QMutex>
#include <QTimer>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QDebug>

#include <algorithm>
#include <cstring>
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

// 文本解析期的临时结构：解析完即序列化成扁平 blob 后释放，不再进查询路径。
// （原 T9Entry/T9PinyinData 曾暴露在头文件里，2026-11-13 扁平化后收进 .cpp。）
struct ParsedEntry
{
    QString word;
    QString pinyin;
    QByteArray digits;
    QByteArray abbr;
    float weight = 0;     // 谷歌表词频；其余来源为 0
    quint8 source = 0;    // 0 谷歌表 / 1 单字表 / 2 词组表（K4 排序次序）
};

struct ParsedData
{
    QList<ParsedEntry> entries;
    QHash<QByteArray, QList<int>> byPrefix2;   // digits 前 2 位 -> 下标
    QHash<QByteArray, QList<int>> abbrPrefix2; // abbr 前 2 位 -> 下标
};

void addEntry(ParsedData &data, const QString &word, const QString &pinyin,
              float weight, quint8 source)
{
    const QString py = pinyin.simplified().toLower();
    if (word.isEmpty() || py.isEmpty())
        return;

    ParsedEntry e;
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

void loadGoogle(ParsedData &data)
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

void loadSingle(ParsedData &data)
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

void loadPhrase(ParsedData &data)
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

namespace {

/*
 * 扁平词库缓存格式 v1（2026-11-13，开机提速 B 方案）。
 *
 * 原加载路径每次启动把 12MB 文本字典解析成 ~50 万条 QString/QByteArray 条目
 * （板上实测 5.5s、常驻 ~150MB）。这里把解析结果序列化成定宽、小端、无指针的
 * 扁平 blob：首启解析一次并原子写缓存，之后启动 mmap + 全量校验即可（几十 ms），
 * 内存按需页入。文本解析与缓存加载共用同一查询路径（都 wrap 成下面的视图）。
 *
 * 布局（全部 4/8 字节自然对齐，偏移由头部计数推出）：
 *   CacheHeader(128) | fullBuckets[72] | abbrBuckets[72] | records[N]
 *   | fullIdx[F] | abbrIdx[A] | pool[P]
 * 桶键 = digits/abbr 的前 1~2 位（数字 2..9）：1 位键 8 个 + 2 位键 64 个。
 * records 保持解析插入序（K4 排序的同分 tie-break 用下标），桶内索引同序。
 * 指纹 = 宿主 exe 的 size+mtime（字典是 qrc 编进 exe 的，换字典=换 exe）+ 版本号。
 */
const quint32 kCacheMagic = 0x31433954u;   // "T9C1"
const quint32 kCacheVersion = 1;
const quint32 kEndianCheck = 0x01020304u;
const int kBucketCount = 72;

struct CacheHeader
{
    quint32 magic;          // kCacheMagic
    quint32 version;        // kCacheVersion
    quint32 endian;         // kEndianCheck，跨字节序保险
    quint32 entryCount;
    quint32 poolBytes;
    quint32 fullIdxCount;
    quint32 abbrIdxCount;
    quint32 headerBytes;    // = sizeof(CacheHeader)，前向兼容用
    qint64 exeSize;         // 指纹
    qint64 exeMTimeMs;      // 指纹
    quint32 reserved[20];
};
static_assert(sizeof(CacheHeader) == 128, "T9 cache header must stay 128 bytes");

struct Bucket
{
    quint32 start;          // 索引起点（fullIdx/abbrIdx 内）
    quint32 count;
};
static_assert(sizeof(Bucket) == 8, "T9 bucket must stay 8 bytes");

struct EntryRec
{
    quint32 wordOff, wordLen;    // pool 内 UTF-8 字节
    quint32 pyOff, pyLen;
    quint32 digOff, digLen;
    quint32 abbrOff, abbrLen;
    float weight;
    quint8 source;
    quint8 pad[3];
};
static_assert(sizeof(EntryRec) == 40, "T9 entry record must stay 40 bytes");

// 桶键 -> 槽位；键只能是 1~2 位 '2'..'9'（letterToDigit 的值域）
int bucketSlot(const QByteArray &key)
{
    if (key.size() == 1)
        return key.at(0) - '2';
    if (key.size() == 2)
        return 8 + (key.at(0) - '2') * 8 + (key.at(1) - '2');
    return -1;
}

} // namespace

namespace AeaQt {

// 引擎全部索引的扁平视图：指向 mmap 缓存或内存 blob，构建完成后只读。
struct T9PinyinData
{
    ~T9PinyinData()
    {
        if (mapped)
            cacheFile.unmap(const_cast<uchar *>(mapped));
    }

    QFile cacheFile;          // mmap 模式：持有映射生命周期
    const uchar *mapped = nullptr;
    QByteArray blob;          // 内存模式：首启解析后 / 缓存不可写时持有 blob

    const EntryRec *records = nullptr;
    const Bucket *fullBuckets = nullptr;
    const Bucket *abbrBuckets = nullptr;
    const quint32 *fullIdx = nullptr;
    const quint32 *abbrIdx = nullptr;
    const char *pool = nullptr;
    quint32 entryCount = 0;
    quint32 poolBytes = 0;
};

} // namespace AeaQt

namespace {

// 指纹取宿主 exe 的 size+mtime：qrc 字典随 exe 走，stat 一次零读取成本。
// 取不到（无 QCoreApplication 等）返回 false，调用方跳过缓存。
bool exeFingerprint(qint64 &size, qint64 &mtimeMs)
{
    const QString path = QCoreApplication::applicationFilePath();
    if (path.isEmpty())
        return false;
    const QFileInfo fi(path);
    if (!fi.exists())
        return false;
    size = fi.size();
    mtimeMs = fi.lastModified().toMSecsSinceEpoch();
    return true;
}

QString cachePath()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty())
        return QString();
    return dir + QStringLiteral("/t9dict-v1.bin");
}

// 把一段扁平 blob（mmap 或内存）wrap 成查询视图，并做全量边界校验。
// 任何一项不符即返回 false（调用方回退文本解析），绝不让损坏缓存进查询路径。
// 校验是 O(条目数) 的纯内存比较，板上几十 ms 量级，远小于解析本身。
bool wrapFlat(T9PinyinData &d, const char *base, qint64 size, bool checkFingerprint)
{
    if (size < qint64(sizeof(CacheHeader)))
        return false;
    CacheHeader h;
    std::memcpy(&h, base, sizeof h);
    if (h.magic != kCacheMagic || h.version != kCacheVersion
        || h.endian != kEndianCheck || h.headerBytes != sizeof(CacheHeader))
        return false;
    if (h.entryCount > 10000000u)
        return false;
    if (checkFingerprint) {
        qint64 sz = 0, mt = 0;
        if (!exeFingerprint(sz, mt) || sz != h.exeSize || mt != h.exeMTimeMs)
            return false;
    }

    const qint64 bucketsOff = sizeof(CacheHeader);
    const qint64 recordsOff = bucketsOff + 2 * kBucketCount * qint64(sizeof(Bucket));
    const qint64 fullIdxOff = recordsOff + qint64(h.entryCount) * sizeof(EntryRec);
    const qint64 abbrIdxOff = fullIdxOff + qint64(h.fullIdxCount) * 4;
    const qint64 poolOff = abbrIdxOff + qint64(h.abbrIdxCount) * 4;
    if (poolOff + h.poolBytes != size)   // 严格等长：截断/拼接一律拒收
        return false;

    d.fullBuckets = reinterpret_cast<const Bucket *>(base + bucketsOff);
    d.abbrBuckets = d.fullBuckets + kBucketCount;
    d.records = reinterpret_cast<const EntryRec *>(base + recordsOff);
    d.fullIdx = reinterpret_cast<const quint32 *>(base + fullIdxOff);
    d.abbrIdx = reinterpret_cast<const quint32 *>(base + abbrIdxOff);
    d.pool = base + poolOff;
    d.entryCount = h.entryCount;
    d.poolBytes = h.poolBytes;

    for (int b = 0; b < kBucketCount; ++b) {
        const Bucket &fb = d.fullBuckets[b];
        const Bucket &ab = d.abbrBuckets[b];
        if (fb.start > h.fullIdxCount || fb.count > h.fullIdxCount - fb.start)
            return false;
        if (ab.start > h.abbrIdxCount || ab.count > h.abbrIdxCount - ab.start)
            return false;
    }
    for (quint32 k = 0; k < h.fullIdxCount; ++k)
        if (d.fullIdx[k] >= h.entryCount)
            return false;
    for (quint32 k = 0; k < h.abbrIdxCount; ++k)
        if (d.abbrIdx[k] >= h.entryCount)
            return false;
    for (quint32 i = 0; i < h.entryCount; ++i) {
        const EntryRec &r = d.records[i];
        if (r.wordOff > h.poolBytes || r.wordLen > h.poolBytes - r.wordOff)
            return false;
        if (r.pyOff > h.poolBytes || r.pyLen > h.poolBytes - r.pyOff)
            return false;
        if (r.digOff > h.poolBytes || r.digLen > h.poolBytes - r.digOff)
            return false;
        if (r.abbrOff > h.poolBytes || r.abbrLen > h.poolBytes - r.abbrOff)
            return false;
    }
    return true;
}

// 缓存命中路径：open + mmap + 校验。失败一律返回空指针，调用方走文本解析。
QSharedPointer<T9PinyinData> tryLoadCache()
{
    const QString path = cachePath();
    if (path.isEmpty())
        return QSharedPointer<T9PinyinData>();
    QSharedPointer<T9PinyinData> d(new T9PinyinData);
    d->cacheFile.setFileName(path);
    if (!d->cacheFile.open(QIODevice::ReadOnly))
        return QSharedPointer<T9PinyinData>();
    const qint64 fsize = d->cacheFile.size();
    uchar *map = d->cacheFile.map(0, fsize);
    if (!map) {
        d->cacheFile.close();
        return QSharedPointer<T9PinyinData>();
    }
    if (!wrapFlat(*d, reinterpret_cast<const char *>(map), fsize, true)) {
        d->cacheFile.unmap(map);
        d->cacheFile.close();
        return QSharedPointer<T9PinyinData>();
    }
    d->mapped = map;
    return d;
}

// 把文本解析结果序列化成扁平 blob。桶内索引保持解析插入序（与原 QList 一致）。
QByteArray serializeFlat(const ParsedData &p, qint64 exeSize, qint64 exeMTimeMs)
{
    const quint32 n = quint32(p.entries.size());
    quint32 fullCount[kBucketCount] = {0};
    quint32 abbrCount[kBucketCount] = {0};
    for (auto it = p.byPrefix2.constBegin(); it != p.byPrefix2.constEnd(); ++it) {
        const int s = bucketSlot(it.key());
        if (s >= 0)
            fullCount[s] = quint32(it->size());
    }
    for (auto it = p.abbrPrefix2.constBegin(); it != p.abbrPrefix2.constEnd(); ++it) {
        const int s = bucketSlot(it.key());
        if (s >= 0)
            abbrCount[s] = quint32(it->size());
    }
    quint32 fTotal = 0, aTotal = 0;
    for (int s = 0; s < kBucketCount; ++s) {
        fTotal += fullCount[s];
        aTotal += abbrCount[s];
    }

    // 字符串池 + 记录单趟构建：word/pinyin 转 UTF-8，digits/abbr 本就是字节。
    // 记录直接 append 进 QByteArray（Qt5 QList 大元素是节点式存储，不连续，
    // 不能当数组 memcpy；QByteArray 保证连续）。
    QByteArray pool;
    pool.reserve(int(n) * 48);
    QByteArray recBlob;
    recBlob.reserve(int(n * sizeof(EntryRec)));
    for (const ParsedEntry &e : p.entries) {
        EntryRec r;
        const QByteArray w = e.word.toUtf8();
        const QByteArray py = e.pinyin.toUtf8();
        r.wordOff = quint32(pool.size());
        r.wordLen = quint32(w.size());
        pool.append(w);
        r.pyOff = quint32(pool.size());
        r.pyLen = quint32(py.size());
        pool.append(py);
        r.digOff = quint32(pool.size());
        r.digLen = quint32(e.digits.size());
        pool.append(e.digits);
        r.abbrOff = quint32(pool.size());
        r.abbrLen = quint32(e.abbr.size());
        pool.append(e.abbr);
        r.weight = e.weight;
        r.source = e.source;
        r.pad[0] = r.pad[1] = r.pad[2] = 0;
        recBlob.append(reinterpret_cast<const char *>(&r), int(sizeof r));
    }

    CacheHeader h;
    std::memset(&h, 0, sizeof h);
    h.magic = kCacheMagic;
    h.version = kCacheVersion;
    h.endian = kEndianCheck;
    h.entryCount = n;
    h.poolBytes = quint32(pool.size());
    h.fullIdxCount = fTotal;
    h.abbrIdxCount = aTotal;
    h.headerBytes = sizeof(CacheHeader);
    h.exeSize = exeSize;
    h.exeMTimeMs = exeMTimeMs;

    QByteArray blob;
    blob.reserve(int(sizeof(CacheHeader) + 2 * kBucketCount * sizeof(Bucket)
                     + qint64(n) * sizeof(EntryRec) + qint64(fTotal + aTotal) * 4
                     + pool.size()));
    blob.append(reinterpret_cast<const char *>(&h), int(sizeof h));
    const int fullTabOff = blob.size();
    blob.append(QByteArray(2 * kBucketCount * int(sizeof(Bucket)), '\0'));
    blob.append(recBlob);
    const int fullIdxOff = blob.size();
    blob.append(QByteArray(int(fTotal * 4), '\0'));
    const int abbrIdxOff = blob.size();
    blob.append(QByteArray(int(aTotal * 4), '\0'));
    blob.append(pool);

    // 回填桶表与索引。blob 已一次性 reserve，上面 append 不会 realloc，指针稳定。
    Bucket *ft = reinterpret_cast<Bucket *>(blob.data() + fullTabOff);
    Bucket *at = ft + kBucketCount;
    quint32 *fi = reinterpret_cast<quint32 *>(blob.data() + fullIdxOff);
    quint32 *ai = reinterpret_cast<quint32 *>(blob.data() + abbrIdxOff);
    quint32 fCursor[kBucketCount];
    quint32 aCursor[kBucketCount];
    quint32 fStart = 0, aStart = 0;
    for (int s = 0; s < kBucketCount; ++s) {
        ft[s].start = fStart;
        ft[s].count = fullCount[s];
        fCursor[s] = fStart;
        fStart += fullCount[s];
        at[s].start = aStart;
        at[s].count = abbrCount[s];
        aCursor[s] = aStart;
        aStart += abbrCount[s];
    }
    for (auto it = p.byPrefix2.constBegin(); it != p.byPrefix2.constEnd(); ++it) {
        const int s = bucketSlot(it.key());
        if (s < 0)
            continue;
        for (int idx : it.value())
            fi[fCursor[s]++] = quint32(idx);
    }
    for (auto it = p.abbrPrefix2.constBegin(); it != p.abbrPrefix2.constEnd(); ++it) {
        const int s = bucketSlot(it.key());
        if (s < 0)
            continue;
        for (int idx : it.value())
            ai[aCursor[s]++] = quint32(idx);
    }
    return blob;
}

// 原子写缓存：QSaveFile 临时文件 + rename，写一半断电不会留下半截缓存。
bool writeCache(const QByteArray &blob, const QString &path)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    if (f.write(blob) != blob.size()) {
        f.cancelWriting();
        return false;
    }
    return f.commit();
}

} // namespace

QSharedPointer<const T9PinyinData> T9PinyinEngine::buildData()
{
    // 1) 缓存命中：mmap + 全量校验，几十 ms（替代原 ~5.5s 文本解析）
    QSharedPointer<T9PinyinData> cached = tryLoadCache();
    if (!cached.isNull()) {
        qInfo() << "T9: 词库缓存命中，" << cached->entryCount << "条（mmap）";
        return cached;
    }

    // 2) 首启 / 缓存失效：文本解析（原路径，后台线程）
    QElapsedTimer timer;
    timer.start();
    ParsedData parsed;
    parsed.entries.reserve(500000);
    syllableIndex();   // 顺带在后台把音节表建好，避免首次按键时在 GUI 线程读文件
    loadGoogle(parsed);
    loadSingle(parsed);
    loadPhrase(parsed);
    if (parsed.entries.isEmpty())
        qWarning() << "T9: 三个字典全部为空，中文候选不可用（检查 qrc 资源路径/alias）";

    // 3) 序列化 + 原子写缓存；写失败也照常用内存 blob（下次启动再试）
    qint64 exeSize = 0, exeMTime = 0;
    const bool haveFingerprint = exeFingerprint(exeSize, exeMTime);
    QByteArray blob = serializeFlat(parsed, exeSize, exeMTime);
    const QString path = cachePath();
    if (haveFingerprint && !path.isEmpty()) {
        if (writeCache(blob, path))
            qInfo() << "T9: 词库缓存已写入" << path << blob.size()
                    << "字节；解析+序列化" << timer.elapsed() << "ms";
        else
            qWarning() << "T9: 词库缓存写入失败（本次用内存词库）：" << path;
    }

    QSharedPointer<T9PinyinData> d(new T9PinyinData);
    d->blob = std::move(blob);
    if (!wrapFlat(*d, d->blob.constData(), d->blob.size(), false)) {
        // 自建 blob 自校验失败属编码 bug，不能静默：清空视图，候选退化为空
        qWarning() << "T9: 扁平词库自校验失败（不应发生），中文候选不可用";
        d->blob.clear();
        d->records = nullptr;
        d->fullBuckets = nullptr;
        d->abbrBuckets = nullptr;
        d->fullIdx = nullptr;
        d->abbrIdx = nullptr;
        d->pool = nullptr;
        d->entryCount = 0;
        d->poolBytes = 0;
    }
    return d;
}

namespace {

/*
 * 进程级共享词库（2026-11-09 AeroCount「拼音第一次输入很卡」的根因修复）。
 *
 * 实测：三本字典（词组表 8MB + 谷歌表 3.5MB + 单字表）解析一份要约 0.5s
 * （开发机 x86；RK3566 上 1~3s），解析完的索引约 150MB，而解析完成后的候选
 * 查询只要 0~2ms——首次卡顿全是加载本身。宿主会在 main() 里先建一个"预热"
 * 引擎把加载提前到启动最早期（主窗还没构造），键盘自己那个引擎晚约 200ms 才建：
 * 两边各解一遍既慢、又各占 150MB。这里让后建的实例直接复用先建的那一份——
 * 已有数据立即就绪，已有实例在加载则轮询等它发布。
 *
 * 访问都在 GUI 线程（loadAsync 与加载完成后的回调都是），仍加锁防哪个宿主把
 * 引擎建到工作线程上。
 */
QMutex g_sharedMutex;
QSharedPointer<const T9PinyinData> g_sharedData;
bool g_sharedLoading = false;

QSharedPointer<const T9PinyinData> sharedData()
{
    QMutexLocker locker(&g_sharedMutex);
    return g_sharedData;
}

void publishSharedData(const QSharedPointer<const T9PinyinData> &data)
{
    QMutexLocker locker(&g_sharedMutex);
    g_sharedData = data;
}

bool claimSharedLoading()
{
    QMutexLocker locker(&g_sharedMutex);
    if (g_sharedLoading)
        return false;
    g_sharedLoading = true;
    return true;
}

void releaseSharedLoading()
{
    QMutexLocker locker(&g_sharedMutex);
    g_sharedLoading = false;
}

} // namespace

void T9PinyinEngine::loadSync()
{
    m_data = buildData();
    m_loading = false;
    publishSharedData(m_data);   // 同步加载也发布：后续实例不必再解一遍
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

bool T9PinyinEngine::adoptSharedData()
{
    const QSharedPointer<const T9PinyinData> shared = sharedData();
    if (shared.isNull())
        return false;
    m_data = shared;
    m_loading = false;
    emit ready();
    return true;
}

void T9PinyinEngine::loadAsync()
{
    if (m_loading || isReady())
        return;
    // 已有别的实例加载过：直接复用，连后台线程都不起
    if (adoptSharedData())
        return;
    if (!claimSharedLoading()) {
        /*
         * 已有实例在加载：轮询等它发布，别重复解一遍 12MB 字典。
         * 轮询而不是"等对方通知"：不需要在模板里加静态信号表，100ms 一级、
         * 加载完最多多等一级，代价可以忽略；引擎若先被销毁，singleShot 的
         * context(this) 会自动取消，不会野掉。
         */
        QTimer::singleShot(100, this, [this] { loadAsync(); });
        return;
    }
    m_loading = true;

    // 后台线程只构建局部对象，经 queued 调用回到本对象所在线程再一次性替换（T8）
    auto *thread = new T9LoadThread(this, [this](QSharedPointer<const T9PinyinData> data) {
        QMetaObject::invokeMethod(this, [this, data] {
            publishSharedData(data);   // 先发布：后建的实例（含轮询中的）立刻能复用
            releaseSharedLoading();
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

// 零拷贝字节视图：pool 里的 UTF-8/数字串，生命周期随 T9PinyinData
inline QByteArray flatBytes(const T9PinyinData &d, quint32 off, quint32 len)
{
    return QByteArray::fromRawData(d.pool + off, int(len));
}

// 桶查找：键非法/空桶返回 count=0 的静态空桶
const Bucket &fullBucket(const T9PinyinData &d, const QByteArray &key)
{
    static const Bucket empty = {0, 0};
    const int s = bucketSlot(key);
    return s < 0 ? empty : d.fullBuckets[s];
}

const Bucket &abbrBucket(const T9PinyinData &d, const QByteArray &key)
{
    static const Bucket empty = {0, 0};
    const int s = bucketSlot(key);
    return s < 0 ? empty : d.abbrBuckets[s];
}

// 分词符位置（去分词符后的数字下标）都必须落在该词的音节边界上。
// 字节版：pinyin 已 simplified，按空格切音节与 QString::split 等价。
bool separatorsAligned(const QByteArray &pinyin, const QList<int> &seps, int limit)
{
    if (seps.isEmpty())
        return true;
    QList<int> bounds;
    int acc = 0;
    int start = 0;
    while (start < pinyin.size()) {
        int sp = pinyin.indexOf(' ', start);
        if (sp < 0)
            sp = pinyin.size();
        if (sp > start) {
            acc += sp - start;
            bounds.append(acc);
        }
        start = sp + 1;
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
    if (!m_data || !m_data->records || limit <= 0)
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

    // locked 是 ASCII 拼音前缀：逐字节比对（跳过拼音里的空格），不构造 QString
    auto lockedOk = [&locked](const QByteArray &py) {
        if (locked.isEmpty())
            return true;
        int li = 0;
        for (char c : py) {
            if (c == ' ')
                continue;
            if (li >= locked.size())
                return true;
            if (c != locked.at(li).toLatin1())
                return false;
            ++li;
        }
        return li >= locked.size();
    };

    auto scanFull = [&](const Bucket &bucket) {
        for (quint32 k = 0; k < bucket.count; ++k) {
            const quint32 i = data.fullIdx[bucket.start + k];
            const EntryRec &r = data.records[i];
            const QByteArray dig = flatBytes(data, r.digOff, r.digLen);
            quint8 cat;
            int consumed;
            if (dig == d) {
                cat = 0; consumed = d.size();
            } else if (dig.startsWith(d)) {
                // 只输一位时只出单字，免得几万条词组参与排序
                if (d.size() == 1 && flatBytes(data, r.pyOff, r.pyLen).contains(' '))
                    continue;
                cat = 2; consumed = d.size();
            } else if (d.startsWith(dig)) {
                cat = 4; consumed = dig.size();
            } else {
                continue;
            }
            const QByteArray py = flatBytes(data, r.pyOff, r.pyLen);
            if (!separatorsAligned(py, seps, consumed) || !lockedOk(py))
                continue;
            hits.append({int(i), cat, consumed});
        }
    };

    auto scanAbbr = [&](const Bucket &bucket) {
        for (quint32 k = 0; k < bucket.count; ++k) {
            const quint32 i = data.abbrIdx[bucket.start + k];
            const EntryRec &r = data.records[i];
            const QByteArray ab = flatBytes(data, r.abbrOff, r.abbrLen);
            quint8 cat;
            if (ab == d)
                cat = 1;
            else if (ab.startsWith(d))
                cat = 3;
            else
                continue;
            if (!lockedOk(flatBytes(data, r.pyOff, r.pyLen)))
                continue;
            hits.append({int(i), cat, int(d.size())});
        }
    };

    if (d.size() == 1) {
        scanFull(fullBucket(data, d));
        for (char c = '2'; c <= '9'; ++c)
            scanFull(fullBucket(data, d + c));
    } else {
        scanFull(fullBucket(data, d.left(2)));
        scanFull(fullBucket(data, d.left(1)));   // 单字母音节（a/e/o…）
        scanAbbr(abbrBucket(data, d.left(2)));
    }

    // K4：类别 > 来源（谷歌/单字/词组）> 词频 > 文件顺序；"只占前段"的词占得越多越靠前
    auto less = [&data](const Hit &a, const Hit &b) {
        if (a.category != b.category)
            return a.category < b.category;
        if (a.consumed != b.consumed)
            return a.consumed > b.consumed;
        const EntryRec &ra = data.records[a.index];
        const EntryRec &rb = data.records[b.index];
        if (ra.source != rb.source)
            return ra.source < rb.source;
        if (ra.weight != rb.weight)
            return ra.weight > rb.weight;
        return a.index < b.index;
    };
    // 同一个词会从多张表命中，先多排一些再去重
    const int sortCount = qMin(hits.size(), limit * 8);
    std::partial_sort(hits.begin(), hits.begin() + sortCount, hits.end(), less);

    QSet<QString> seen;
    for (int k = 0; k < sortCount && result.size() < limit; ++k) {
        const EntryRec &r = data.records[hits.at(k).index];
        // 词面只在进候选时才构造 QString（去重 + 上屏），扫描热路径零分配
        const QString word = QString::fromUtf8(data.pool + r.wordOff, int(r.wordLen));
        if (seen.contains(word))
            continue;
        seen.insert(word);
        // 显式逐字段构造：T9Candidate 带默认成员初始值，C++11 下不是聚合，
        // 不能 {word, n} 列表初始化（模板标称 C++11，Qt5 线按 c++11 编）
        T9Candidate c;
        c.word = word;
        c.digitCount = hits.at(k).consumed;
        result.append(c);
    }
    return result;
}
