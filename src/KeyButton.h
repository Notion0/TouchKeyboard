// Keyboard/KeyButton.h
// 键盘按键控件：自绘键帽，支持普通键/特殊键（切换/退格/空格等）长按连发。
// 键帽视觉（白底/圆角/按下态）全部由 paintEvent 画，不用 setStyleSheet——
// 取色与字号见 AeaKeyboardStyle.h。
#ifndef AEA_KEY_BUTTON_H
#define AEA_KEY_BUTTON_H

#include <QPushButton>
#include <QVariant>

namespace AeaQt {

class KeyButton : public QPushButton
{
    Q_OBJECT
public:
    enum Type { Auto = 0, LowerCase, UpperCase, SpecialChar };

    struct Mode {
        Mode() { }
        Mode(int _key, QString _value, QVariant _display = QString(), Type _type = Auto)
        {
            key     = _key;
            value   = _value;
            display = _display;
            type    = _type;
        }

        int key;          /* Qt::Key */
        QString value;    /* text */
        QVariant display;  /* display text or icon */
        Type type;        /* default: Auto */
    };

    /// @brief 多形态按键构造（主/大写/符号等形态集）
    KeyButton(const QList<Mode> modes = QList<Mode>(), QWidget *parent = nullptr);
    /// @brief 当前形态
    Mode mode();

    /// @brief 绑定按键接收者（转发 pressed 信号）
    void onReponse(const QObject* receiverObj, const QString &receiver);

    /// @brief 键帽像素字号（宿主按字号系数统一下发；0 = 模板默认 kCapDefaultPixels）
    void setCapPixels(int px);

    /// @brief 切大写形态
    void switchCapsLock();
    /// @brief 切符号形态
    void switchSpecialChar();
    void switching(); /* Cycle switch. */

signals:
    /// @brief 按键信号（码+文本），由接收方写入目标控件
    void pressed(int key, QString value);

private slots:
    /// @brief 点击处理：按形态发 pressed
    void onPressed();

private:
    /// @brief 文本→键类型
    Type find(const QString &value);
    /// @brief 键类型→形态
    Mode find(Type type);
    /// @brief 循环切换到下一形态
    Mode findNext();
    /// @brief 设置键帽显示内容
    void setDisplayContent(const QVariant &content);

protected:
    /// @brief 自绘键帽：底色/圆角/按下态/文字全部画在这里（模板禁 QSS，
    ///        原 DEFAULT_STYLE_SHEET 的等价实现）
    void paintEvent(QPaintEvent *event) override;

private:
    /// @brief 把文字缩到放得进键帽（不超过当前字号）；图标键不参与
    int fitTextPixels(const QRectF &cap) const;

private:
    Mode        m_preMode;
    Mode        m_mode;
    QList<Mode> m_modes;
    int         m_capPixels = 0;   // 0 = 未下发（用 kCapDefaultPixels）
};

}
#endif // AEA_KEY_BUTTON_H
