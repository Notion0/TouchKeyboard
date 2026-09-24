// InputManager.h
// 输入管理单例：全局事件过滤，行编辑获得焦点时弹出对应键盘——
// numericInput 动态属性为真弹数字小键盘（0-9/./退格），否则弹全键盘（字母/符号/中文候选），
// 失焦或点击非输入区自动收起。
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
