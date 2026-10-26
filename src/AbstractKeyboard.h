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
#include <QAbstractSpinBox>
#include <QLineEdit>
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

/// 数值输入整选（自 aerocount 分叉线合回，2026-10-26）：数字小键盘弹出前对宿主控件
/// 整段选中数值，之后首个按键即整段替换旧值、退格一次删完整段数字——免去「先逐个
/// 删旧值再输」。QAbstractSpinBox 焦点实际落在内嵌行编辑上，取其本体：
/// QAbstractSpinBox::selectAll 只选数值段、不含 prefix/suffix（"300 mbar" 只选
/// "300"）；对行编辑直接 selectAll 会把单位后缀揽进选区。纯 QLineEdit 数值框全文即
/// 数字，取自身。非数值控件返回 false（不适用）
inline bool selectAllNumericContent(QWidget *focusWidget)
{
    if (!focusWidget)
        return false;

    if (auto *spin = qobject_cast<QAbstractSpinBox *>(focusWidget)) {
        spin->selectAll();
        return true;
    }
    if (auto *edit = qobject_cast<QLineEdit *>(focusWidget)) {
        if (auto *spin = qobject_cast<QAbstractSpinBox *>(edit->parentWidget())) {
            spin->selectAll();
            return true;
        }
        edit->selectAll();
        return true;
    }
    return false;
}

/// 圆角窗口遮罩：QSS 的 border-radius 只裁内容不裁窗口，定角会露出底层内容显成方角。
/// 键盘（含嵌作宿主子控件的形态）按此生成 mask，由窗口/控件层裁出真圆角。
/// @param size   窗口尺寸
/// @param radius 圆角半径（与面板 QSS 的 border-radius 一致）
inline QRegion roundedWindowMask(const QSize &size, int radius)
{
    QPainterPath path;
    path.addRoundedRect(QRect(QPoint(0, 0), size), radius, radius);
    return QRegion(path.toFillPolygon().toPolygon());
}

/// 面板圆角半径（px）：两键盘 QSS border-radius 与圆角 mask 的唯一来源，
/// 三处（Keyboard.cpp / NumberKeyboard.cpp / 两处 setMask）必须与此一致
constexpr int kCornerRadius = 15;

class AbstractKeyboard : public QWidget
{
    Q_OBJECT
public:
    AbstractKeyboard(QWidget *parent = nullptr) : QWidget(parent) {
        // 面板 QSS 底色（background-color）只在顶层窗口被自动绘制；嵌入主窗作
        // 子控件后，普通 QWidget 子类必须显式开 WA_StyledBackground 才画 QSS
        // 背景——否则面板透明只剩键帽（QFrame 面板不受此限，普通 QWidget 面板必设）
        setAttribute(Qt::WA_StyledBackground, true);
    }
    ~AbstractKeyboard() { }

    // ---- 文本键盘公共接口（26 键 Keyboard / 九宫格 T9Keyboard 实现；数字小键盘用默认空实现）----
    // InputManager 只经这几个接口驱动文本键盘，两种布局对宿主透明。

    /// @brief 英文-only（asciiOnly 字段）：强制英文态并让中/英切换失效
    virtual void setEnglishOnly(bool enabled) { Q_UNUSED(enabled); }
    /// @brief 当前是否中文态（布局切换时把中英态带到另一个键盘）
    virtual bool isChineseMode() const { return false; }
    /// @brief 设置中英态（英文-only 下请求中文会被忽略）
    virtual void setChineseMode(bool chinese) { Q_UNUSED(chinese); }

signals:
    /// @brief 用户点了「九宫格/全键盘」布局切换键；由 InputManager 接管实际切换
    void layoutSwitchRequested();

public slots:
    /// @brief 把一次按键以 QKeyEvent（press+release）发给当前焦点控件
    void onKeyPressed(int key, QString value)
    {
        QWidget *receiver = QApplication::focusWidget();
        if (!receiver)
            return;

        QKeyEvent keyPress(QEvent::KeyPress,     key, Qt::NoModifier, value);
        QKeyEvent keyRelease(QEvent::KeyRelease, key, Qt::NoModifier, value);

        QApplication::sendEvent(receiver, &keyPress);
        QApplication::sendEvent(receiver, &keyRelease);
    }

private:
};

}
#endif // AEA_ABSTRACT_KEYBOARD_H
