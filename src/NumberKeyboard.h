// Keyboard/NumberKeyboard.h
// 数字小键盘：纯数字输入行编辑专用（点击弹出），仅 0-9 + 小数点 + 退格。
// 由 InputManager 按行编辑的 numericInput 动态属性选弹，替代全键盘。
/**********************************************************
#Author: Qtjun
#WeChat Official Accounts: qthub_com
#QQ Group: 732271126
#Email: 2088201923@qq.com
**********************************************************/

#ifndef NUMBERKEYBOARD_H
#define NUMBERKEYBOARD_H

#include "AbstractKeyboard.h"
#include "KeyButton.h"
#include <QLayout>
#include <QString>

namespace AeaQt {

class NumberKeyboard : public AbstractKeyboard
{
    Q_OBJECT
public:
    NumberKeyboard(QWidget *parent = NULL);

    KeyButton *createButton(QList<KeyButton::Mode> modes);

protected:
    /// @brief 尺寸变化：图标键帽的图标尺寸随键帽重算（构造期尺寸未定会被钉死）
    void resizeEvent(QResizeEvent *e);

private slots:
    void onButtonPressed(const int &code, const QString &text);
};

}

#endif // NUMBERKEYBOARD_H
