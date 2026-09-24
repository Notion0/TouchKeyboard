// Keyboard/AbstractKeyboard.h
// 触摸键盘抽象基类：按键布局与发送按键事件的公共骨架，具体键盘（中文/数字）继承实现。
/**********************************************************
#Author: Qtjun
#WeChat Official Accounts: qthub_com
#QQ Group: 732271126
#Email: 2088201923@qq.com
**********************************************************/

#ifndef AEA_ABSTRACT_KEYBOARD_H
#define AEA_ABSTRACT_KEYBOARD_H

#include <QApplication>
#include <QWidget>
#include <QDebug>
#include <QKeyEvent>
#include <QColor>
#include <QString>
#include <QPainterPath>
#include <QRegion>

namespace AeaQt {

/// 品牌 logo 文本（标题栏显示；两个键盘共用——改此一处即全模板换肤）
inline const QString &brandLogoText()
{
    static const QString s = QStringLiteral("WOWA-无涯");
    return s;
}

/// 品牌面板底色（两个键盘的面板同款；改此一处即全模板换肤）
inline QColor brandPanelColor()
{
    static const QColor c(34, 149, 144);
    return c;
}

/// 圆角窗口遮罩：QSS 的 border-radius 只裁内容不裁窗口，定角会露出底层白底显成方角。
/// 顶部 Tool 窗按此生成 mask 并配 WA_TranslucentBackground，窗口本体才是真圆角。
/// @param size   窗口尺寸
/// @param radius 圆角半径（与面板 QSS 的 border-radius 一致）
inline QRegion roundedWindowMask(const QSize &size, int radius)
{
    QPainterPath path;
    path.addRoundedRect(QRect(QPoint(0, 0), size), radius, radius);
    return QRegion(path.toFillPolygon().toPolygon());
}

class AbstractKeyboard : public QWidget
{
    Q_OBJECT
public:
    AbstractKeyboard(QWidget *parent = 0) : QWidget(parent) {

    }
    ~AbstractKeyboard() { }

    const QString name() { return m_name; }
    void setName(const QString &name) { m_name = name; }

public slots:
    virtual void update(const QString &text) { Q_UNUSED(text); }

    void onKeyPressed(int key, QString value)
    {
        // qDebug() << "key: " << key << "Value: " << value;        //按键按下对应字母输出
        QWidget *receiver = QApplication::focusWidget();
        if (!receiver)
            return;

        QKeyEvent keyPress(QEvent::KeyPress,     key, Qt::NoModifier, value);
        QKeyEvent keyRelease(QEvent::KeyRelease, key, Qt::NoModifier, value);

        QApplication::sendEvent(receiver, &keyPress);
        QApplication::sendEvent(receiver, &keyRelease);
    }

signals:
    void keyPressed(int key, QString value);

private:
    QString m_name;
};

}
#endif // AEA_ABSTRACT_KEYBOARD_H
