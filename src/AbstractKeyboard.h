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
#include <QHash>
#include <QIcon>
#include <QPainter>
#include <QPixmap>

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

/// 键帽图标种类（见 keyIcon）
enum class KeyIcon { Backspace, CapsLock, Enter, Space, Hide };

/// 键帽图标：QPainter 现画矢量、不读任何图片文件。
///
/// 为什么不用 resources/icons/*.png（2026-10-26 aerocount 板上实测）：RK3566 Buildroot
/// 的 Qt 5.15.2 以 -no-libpng 构建（libQt5Gui 不链 libpng、无内置 PNG 读取器，
/// imageformats 插件也只有 ico/svg），QIcon(":/…png") 读出空图——全键盘大小写/退格/
/// 回车/空格与标题栏收起键整块空白，数字小键盘退格同样。SVG 也要靠 iconengines 插件，
/// 宿主不一定装。现画则任何 Qt 构建都能显示。png 资源保留在 qrc 里不再引用（向后兼容）。
/// @param kind  图标种类
/// @param color 线条/填充颜色（默认黑，与原 png 一致）
inline QIcon keyIcon(KeyIcon kind, const QColor &color = Qt::black)
{
    // resizeButton 每次 resize 都会重设图标：按 (种类, 颜色) 缓存，只画一次
    static QHash<quint64, QIcon> cache;
    const quint64 cacheKey = (quint64(kind) << 32) | color.rgba();
    const auto hit = cache.constFind(cacheKey);
    if (hit != cache.constEnd())
        return hit.value();

    const int s = 96;   // 画布边长；QIcon 按 iconSize 缩放，96 足够清晰
    QPixmap pm(s, s);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color, 8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (kind) {
    case KeyIcon::Backspace: {
        // 左尖五边形外框 + 中间 ×
        QPainterPath body;
        body.moveTo(8, 48);
        body.lineTo(30, 20);
        body.lineTo(86, 20);
        body.lineTo(86, 76);
        body.lineTo(30, 76);
        body.closeSubpath();
        p.drawPath(body);
        p.drawLine(QPointF(46, 34), QPointF(70, 62));
        p.drawLine(QPointF(70, 34), QPointF(46, 62));
        break;
    }
    case KeyIcon::CapsLock: {
        // 实心上箭头 + 底横线
        QPainterPath arrow;
        arrow.moveTo(48, 8);
        arrow.lineTo(84, 44);
        arrow.lineTo(62, 44);
        arrow.lineTo(62, 72);
        arrow.lineTo(34, 72);
        arrow.lineTo(34, 44);
        arrow.lineTo(12, 44);
        arrow.closeSubpath();
        p.setBrush(color);
        p.drawPath(arrow);
        p.drawLine(QPointF(32, 86), QPointF(64, 86));
        break;
    }
    case KeyIcon::Enter: {
        // ↵：右上竖线下行、左折，左端箭头
        p.drawLine(QPointF(80, 16), QPointF(80, 60));
        p.drawLine(QPointF(80, 60), QPointF(20, 60));
        p.drawLine(QPointF(20, 60), QPointF(40, 40));
        p.drawLine(QPointF(20, 60), QPointF(40, 80));
        break;
    }
    case KeyIcon::Space: {
        // ⎵
        p.drawLine(QPointF(12, 44), QPointF(12, 64));
        p.drawLine(QPointF(12, 64), QPointF(84, 64));
        p.drawLine(QPointF(84, 64), QPointF(84, 44));
        break;
    }
    case KeyIcon::Hide: {
        // 键盘外框 + 下方收起箭头
        p.drawRoundedRect(QRectF(10, 12, 76, 46), 8, 8);
        QPen thin(color, 6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(thin);
        for (int i = 0; i < 4; ++i)
            p.drawPoint(QPointF(26 + i * 15, 28));
        p.drawLine(QPointF(30, 44), QPointF(66, 44));
        p.setPen(pen);
        p.drawLine(QPointF(34, 72), QPointF(48, 86));
        p.drawLine(QPointF(48, 86), QPointF(62, 72));
        break;
    }
    }
    p.end();
    const QIcon icon(pm);
    cache.insert(cacheKey, icon);
    return icon;
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
