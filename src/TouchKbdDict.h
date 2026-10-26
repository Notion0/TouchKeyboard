// Keyboard/TouchKbdDict.h
// 中文三字典的 qrc 路径与打开兜底：26 键 ChineseWidget 与九宫格 T9PinyinEngine 共用。
// 只依赖 QtCore（T9 引擎单测为 QT -= gui，不能经 AbstractKeyboard.h 引入 Widgets）。
#ifndef AEA_TOUCHKBD_DICT_H
#define AEA_TOUCHKBD_DICT_H

#include <QFile>
#include <QString>

// 中文输入三字典（qrc 路径，随模板 qrc 编入——中文输入是固有能力，非可选项）。
// 资源路径与模板 resources/TouchKeyboard.qrc 的三个 qresource 前缀一一对应
// （qrc 侧必须写 alias 映射成扁平路径，详见 openTouchKbdDict 上方注释）。
// 要换自定义字典：.pro 里 -D 覆盖宏，或直接改这里。
#ifndef TOUCHKBD_PINYIN_DICT
#define TOUCHKBD_PINYIN_DICT       ":/ChineseLib/pinyin.txt"
#endif
#ifndef TOUCHKBD_PINYIN_PHRASE_DICT
#define TOUCHKBD_PINYIN_PHRASE_DICT ":/ChinesePhraseLib/pinyin_phrase.txt"
#endif
#ifndef TOUCHKBD_GOOGLE_DICT
#define TOUCHKBD_GOOGLE_DICT       ":/GoogleChineseLib/rawdict_utf16_65105_freq_sort.txt"
#endif

namespace AeaQt {

// 字典资源打开：扁平路径（qrc 写了 alias 的形态，模板 qrc 现状）优先，
// 打不开再回落「prefix/dicts/文件名」的嵌套路径试一次。
// 缘由：qrc 漏写 alias 时真实路径会多出 /dicts 一节，三个字典 open 全部失败，
// 而失败提示原是 qDebug（发布构建 QT_NO_DEBUG_OUTPUT 下被整段编掉）——静默
// 失效表现为「候选栏只剩原始拼音、中文输入全废」。PUPSIT(2026-09-29)与
// PressureMonitor_noflow(2026-10-09)两次现场反馈同一根因。此处双路径兜底：
// 宿主 qrc 即使漏 alias 也能加载，不把正确性全押在资源配置上。
inline bool openTouchKbdDict(QFile &file, const QString &flatPath)
{
    file.setFileName(flatPath);
    if (file.open(QIODevice::ReadOnly))
        return true;

    const int slash = flatPath.lastIndexOf(QLatin1Char('/'));
    if (slash <= 0)
        return false;
    file.setFileName(flatPath.left(slash) + QStringLiteral("/dicts") + flatPath.mid(slash));
    return file.open(QIODevice::ReadOnly);
}

}

#endif // AEA_TOUCHKBD_DICT_H
