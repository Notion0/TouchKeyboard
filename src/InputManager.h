// InputManager.h
// 输入管理单例：全局事件过滤，行编辑获得焦点时弹出对应键盘——
// numericInput 动态属性为真弹九宫格数字模式（T9Keyboard::Digits），否则弹文本键盘；
// 文本键盘有两种布局并存：26 键全键盘 Keyboard 与九宫格 T9Keyboard，
// 由 setTextLayout() 设默认、键盘上的「九宫格/全键盘」键运行期切换。
// 失焦或点击非输入区自动收起。
//
// ⚠ 接入必读（2026-09-24 起）：键盘是 Init(parent) 传入父窗口的【普通子控件】，
//   不是顶层 Tool 窗—— Weston/X11 下顶层工具窗每次映射都被抢走激活权，主窗失活触发
//   focusChanged(nullptr) 收起、主窗重新激活又还原输入框焦点再弹，形成 show/hide
//   拉锯（界面一直闪、键盘位置飘忽）。挂为子控件后显示不触碰激活态，从结构上根除。
//   代价：键盘不浮出父窗口边界（单全屏宿主即主窗口，无实际影响）。
//   定位：输入框下方，按父窗口本地坐标计算，越界在父窗矩形内上翻/钳制。
#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include <QObject>
#include <QLineEdit>
#include <QTextEdit>
#include <QStringList>
#include <QPointer>
#include "AbstractKeyboard.h"
// 保留传递包含：老宿主经本头间接拿 AeaQt::Keyboard / KeyButton（如按 KeyButton 本地化文案），
// 去掉会让它们编不过
#include "Keyboard.h"

namespace AeaQt {
class T9Keyboard;
class DatePicker;
}

using namespace AeaQt;


class InputManager : public QObject
{
    Q_OBJECT

public:
    /// 文本键盘布局：26 键全键盘 / 九宫格
    enum TextLayout { Qwerty, T9 };

    static InputManager* Instance();

    void Init(QWidget *parent);

    /// @brief 设置文本键盘布局（默认 Qwerty）。Init 前后都可调；键盘上的布局切换键
    ///        也走这里，所以宿主读 textLayout() 即可拿到用户最后选的布局（不持久化，
    ///        需要跨重启记忆由宿主自行存取）
    void setTextLayout(TextLayout layout);
    TextLayout textLayout() const { return m_layout; }

    /// @brief 日期选择器中英文（默认中文）。宿主切换界面语言时调用；
    ///        已弹出的日历下一次 showDatePicker 时生效
    void setChineseLocale(bool chinese);

    /// @brief 宿主注入业务词（账户名/房间名/点位名等），进九宫格英文候选。
    ///        26 键全键盘无英文候选，不使用；Init 前调用会缓存，九宫格创建时应用
    void setExtraWords(const QStringList &words);

signals:
    /// @brief 布局变化（API 设置或用户点切换键）
    void textLayoutChanged(InputManager::TextLayout layout);

private slots:
    void focusChanged(QWidget *old, QWidget *now);
    /// @brief 键盘上的「九宫格/全键盘」键：切到另一种布局并对当前输入框重弹
    void toggleTextLayout();
private:
    explicit InputManager(QObject *parent = nullptr);

    /// @brief 按输入框属性选键盘并定位显示（同时收起其余键盘）
    void showKeyboard(QWidget* now);

    /// @brief 该控件是否属于任一键盘（点击/焦点落在键盘上时不当作外部点击）
    bool isKeyboardWidget(QWidget* w) const;

    /// @brief 所有键盘一并收起
    void hideKeyboards();

    /// @brief 当前布局对应的文本键盘（九宫格按需创建）
    AeaQt::AbstractKeyboard *textKeyboard();
    /// @brief 九宫格按需创建：构造即后台加载词库（约百 MB 级），不用的宿主不付这份内存
    void ensureT9Keyboard();
    /// @brief 新建键盘的公共收尾：无焦点、隐藏、接布局切换信号
    void prepareKeyboard(AeaQt::AbstractKeyboard *kb);

    /// @brief 命中「该控件是不是挂了大日历的日期框」：本体或内嵌行编辑都算
    QDateEdit *resolveDatePickerEdit(QWidget *w) const;
    /// @brief 日期选择器懒创建（不用的宿主不付这份内存）
    void ensureDatePicker();
    /// @brief 为某个日期框弹出/刷新日历并定位（重复点击同一框不重弹，见实现）
    void showDatePicker(QDateEdit *edit);

    AeaQt::Keyboard *keyboard = nullptr;          // 26 键全键盘
    AeaQt::T9Keyboard *t9Keyboard = nullptr;      // 九宫格（含数字模式，懒创建）
    AeaQt::DatePicker *datePicker = nullptr;      // 触屏日期选择器（懒创建）
    QWidget *mainWidget = nullptr;
    QWidget *currentInput = nullptr;
    QPointer<QDateEdit> currentDateEdit;          ///< 正在被日历编辑的日期框
    bool m_chineseLocale = true;                  ///< 日历文案语言
    TextLayout m_layout = Qwerty;
    QStringList m_extraWords;   // 业务词缓存：九宫格创建前到达的先存
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

};

#endif
