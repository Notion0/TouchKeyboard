// InputManager.h
// 输入管理单例：全局事件过滤，行编辑获得焦点时弹出对应键盘——
// numericInput 动态属性为真弹数字小键盘（0-9/./退格），否则弹全键盘（字母/符号/中文候选），
// 失焦或点击非输入区自动收起。
//
// ⚠ 接入必读（2026-09-24 起）：两个键盘是 Init(parent) 传入父窗口的【普通子控件】，
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
#include "Keyboard.h"

namespace AeaQt {
class NumberKeyboard;
}

using namespace AeaQt;


class InputManager : public QObject
{
    Q_OBJECT

public:
    static InputManager* Instance();

    void Init(QWidget *parent);

private slots:
    void focusChanged(QWidget *old, QWidget *now);

    void onKeyboardInput(int code, QString text);
private:
    explicit InputManager(QObject *parent = nullptr);

    /// @brief 按输入框属性选键盘并定位显示（同时收起另一个）
    void showKeyboard(QWidget* now);

    /// @brief 该控件是否属于任一键盘（点击/焦点落在键盘上时不当作外部点击）
    bool isKeyboardWidget(QWidget* w) const;

    /// @brief 两个键盘一并收起
    void hideKeyboards();

    AeaQt::Keyboard *keyboard;
    AeaQt::NumberKeyboard *numberKeyboard;
    QWidget *mainWidget;
    QWidget *currentInput;
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

};

#endif
