// Keyboard/NumberKeyboard.h
// 数字小键盘：纯数字输入行编辑专用（点击弹出），仅 0-9 + 小数点 + 退格。
// 由 InputManager 按行编辑的 numericInput 动态属性选弹，替代全键盘。
#ifndef NUMBERKEYBOARD_H
#define NUMBERKEYBOARD_H

#include "AbstractKeyboard.h"
#include "KeyButton.h"
#include <QLayout>
#include <QString>
class QPainter;
class QPaintEvent;

namespace AeaQt {

class NumberKeyboard : public AbstractKeyboard
{
    Q_OBJECT
public:
    NumberKeyboard(QWidget *parent = nullptr);

    KeyButton *createButton(QList<KeyButton::Mode> modes);

protected:
    /// @brief 尺寸变化：图标键帽的图标尺寸随键帽重算（构造期尺寸未定会被钉死）
    void resizeEvent(QResizeEvent *e);

    /// @brief 面板自绘（禁 QSS 后 background-color/border-radius 的替代：
    ///        品牌青底 + 15px 圆角，不描灰边——与 26 键全键盘的既定差异）
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onButtonPressed(const int &code, const QString &text);
};

}

#endif // NUMBERKEYBOARD_H
