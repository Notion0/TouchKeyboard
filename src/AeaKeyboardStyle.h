// Keyboard/AeaKeyboardStyle.h
// 触摸键盘统一视觉/规格常量表：全模板禁 setStyleSheet 之后，三个键盘（26 键全键盘 /
// 九宫格 / 数字小键盘）的自绘取色、功能键规格、键帽字号漏斗都收在这里——
// 改一处三处生效，禁止在业务代码里散落 #RRGGBAA 魔法值与裸像素字号。
//
// 只放常量与 inline 小函数：模板声明 C++11（TouchKeyboard.pri 顶部），
// 没有 inline 变量，QColor 也不能 constexpr，故颜色一律以 inline 函数暴露，
// 数字类常量用 constexpr。几何类常量（圆角半径、面板底色）在 AbstractKeyboard.h。
#ifndef AEA_KEYBOARD_STYLE_H
#define AEA_KEYBOARD_STYLE_H

#include <QColor>
#include <QPalette>
#include <QWidget>
#include <QtGlobal>

namespace AeaQt {

// ---------------------------------------------------------------------------
// 颜色表（禁 QSS 后自绘取色唯一来源）
// ---------------------------------------------------------------------------
// 键帽底：三键盘一致的白底（原 QSS `background: white`）
inline QColor keyCapFaceColor() { static const QColor c(255, 255, 255); return c; }
// 键帽按下态：26 键/小键盘沿用原 QSS `AeaQt--KeyButton:pressed` 的 #01ddfd；
// 九宫格沿用其自绘值 190,205,204（见 keyCapPressedMutedColor）。两个值都在本表，
// 哪天真要统一只改这两行，别在别处再抄一份
inline QColor keyCapPressedColor() { static const QColor c(0x01, 0xdd, 0xfd); return c; }
inline QColor keyCapPressedMutedColor() { static const QColor c(190, 205, 204); return c; }
// 功能键底（九宫格左列/右列）
inline QColor keyCapFunctionFaceColor() { static const QColor c(222, 238, 237); return c; }
// 选中/高亮底（九宫格模式键高亮、候选条目高亮同族）
inline QColor keyCapActiveFaceColor() { static const QColor c(20, 110, 106); return c; }
// 键帽文字 / 副标文字 / 禁用文字
inline QColor keyCapInkColor() { static const QColor c(51, 51, 51); return c; }
inline QColor keyCapSubInkColor() { static const QColor c(120, 120, 120); return c; }
inline QColor keyCapDisabledInkColor() { static const QColor c(150, 150, 150); return c; }
// 面板描边（原 QSS `border: 1px solid gray`；仅 26 键全键盘带边，小键盘不带）
inline QColor panelBorderColor() { static const QColor c(Qt::gray); return c; }
// 标题栏文字（原 QSS `color: rgb(255, 255, 255)`）
inline QColor titleInkColor() { static const QColor c(255, 255, 255); return c; }
// 候选条目悬停/选中底（原 QSS `QListWidget::Item:hover/selected` 的 #4395ff）
inline QColor candidateHighlightColor() { static const QColor c(0x43, 0x95, 0xff); return c; }
inline QColor candidateHighlightInkColor() { static const QColor c(255, 255, 255); return c; }
inline QColor candidateInkColor() { static const QColor c(Qt::black); return c; }
// 候选条目未选中底：透明（原 QSS `background: #00000000`）
inline QColor candidateIdleFaceColor() { static const QColor c(0, 0, 0, 0); return c; }

// ---------------------------------------------------------------------------
// 键帽几何（自绘用；原 QSS margin / border-radius / font-size 的等价常量）
// ---------------------------------------------------------------------------
constexpr int kCapMargin = 5;         // 键帽内缩（原 QSS `margin: 5px`）
constexpr int kCapRadius = 5;         // 键帽圆角（原 QSS `border-radius: 5px`）
constexpr int kCapTextPadding = 4;    // 文字与键帽边的左右留白（原 QSS padding 口径）
constexpr int kCapDefaultPixels = 26; // 键帽默认像素字号（原 QSS `font-size: 26px`）
constexpr int kCapMinPixels = 8;      // 缩字下限（26 键 fitKeyFont 同口径）
// 收起钮命中区下限（本仓交互控件 44 触控下限）：视觉可小，命中区必须够
constexpr int kCloseButtonSize = 44;
// 26 键标题栏高（原 40；收起钮升到 44 后一起提到 48）
constexpr int kTitleBarHeight = 48;

// ---------------------------------------------------------------------------
// 功能键统一规格表（退格 / 确认 / 清空 / 空格：三键盘共用的唯一事实源）
// ---------------------------------------------------------------------------
// 同语义键在三枚键盘上的统一规格。宽度 = 基准宽；高度由各键盘行高派生
// （九宫格取 kFuncKeyRowHeight；26 键 = (键盘高 − 标题栏)/4；小键盘 = 面板行高），
// 故表里只放宽与形态——高度在各键盘按自身行高取，别在这里写死第二个高度。
enum class KeyCapForm { Text, Icon };

constexpr int kFuncKeyBaseWidth = 92;   // 基准宽 = 九宫格侧键宽
constexpr int kFuncKeyRowHeight = 78;   // 九宫格行高（另两键盘按自身行高派生）

struct FunctionKeySpec {
    int baseWidth;
    KeyCapForm form;
};

// 退格：三键盘统一为文字「退格」（九宫格原本即文字；26 键/小键盘原为图标，
// 2026-11-14 统一成文字——与九宫格这个最新键盘拉齐，操作员不看图标也认得）
constexpr FunctionKeySpec kBackspaceSpec { kFuncKeyBaseWidth, KeyCapForm::Text };
// 确认：三键盘统一为文字「确认」（26 键原为 ↵ 图标；其行为本就是「失焦提交」，
// 写「确认」比 ↵ 更贴近语义）
constexpr FunctionKeySpec kEnterSpec { kFuncKeyBaseWidth, KeyCapForm::Text };
// 清空：三键盘统一为文字「清空」（三键盘原本就是文字，此处只是收进表里）
constexpr FunctionKeySpec kClearSpec { kFuncKeyBaseWidth, KeyCapForm::Text };
// 空格：三键盘统一为文字「空格」（26 键原为 ⎵ 图标）
constexpr FunctionKeySpec kSpaceSpec { kFuncKeyBaseWidth, KeyCapForm::Text };

// ---------------------------------------------------------------------------
// stretch 份数换算（26 键底两行 / 小键盘末行把「目标像素宽」变成布局份数）
// ---------------------------------------------------------------------------
// QHBoxLayout 的 stretch 是整数份数，按份数均分。要把 count 枚统一基准宽的功能键
// 落到 targetWidth 上，就得反解其余键的份数：设其余键份数之和 otherStretchSum
// 已知、行可用宽 rowWidth 已知，则统一键份数 = target × otherSum / (row − count×target)。
// 放不下统一宽（窄宿主/小面板）时退回与其余键同份（均分），不产生 0/负值。
inline int stretchForUnifiedWidth(int targetWidth, int count, int otherStretchSum, int rowWidth)
{
    const int unifiedTotal = targetWidth * count;
    if (unifiedTotal <= 0 || rowWidth <= unifiedTotal)
        return otherStretchSum > 0 ? otherStretchSum : 1;
    return qMax(1, qRound(double(targetWidth) * otherStretchSum / (rowWidth - unifiedTotal)));
}

// ---------------------------------------------------------------------------
// 键帽像素字号漏斗（宿主按字号系数统一下发）
// ---------------------------------------------------------------------------
// 宿主（如 aerocount 的 kFontScale）在创建键盘之前调 setGlobalKeyCapPixels(px)，
// 之后新建的三个键盘都按此渲染；已存在的键盘可逐个调
// AbstractKeyboard::setKeyCapPixels()。未下发（0）时各键盘用自己的默认值，
// 模板行为与下发前完全一致。
inline int &keyCapPixelsStore() { static int px = 0; return px; }
inline void setGlobalKeyCapPixels(int px) { keyCapPixelsStore() = px; }
inline int globalKeyCapPixels() { return keyCapPixelsStore(); }

// ---------------------------------------------------------------------------
// palette 助手
// ---------------------------------------------------------------------------
// 给控件上一套前景色：禁 QSS 之后 `color: xxx` 的唯一替代。QLabel 取 WindowText、
// QPushButton 取 ButtonText、输入框/列表取 Text，一次设全，子孙控件继承
// （候选区的 QListWidget 文字色就是这么来的）。
inline void applyForegroundColor(QWidget *w, const QColor &ink)
{
    if (!w)
        return;
    QPalette pal = w->palette();
    pal.setColor(QPalette::WindowText, ink);
    pal.setColor(QPalette::ButtonText, ink);
    pal.setColor(QPalette::Text, ink);
    pal.setColor(QPalette::ToolTipText, ink);
    w->setPalette(pal);
    w->setAutoFillBackground(false);
}

} // namespace AeaQt

#endif // AEA_KEYBOARD_STYLE_H
