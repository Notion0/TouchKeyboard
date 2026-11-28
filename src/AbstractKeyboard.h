// Keyboard/AbstractKeyboard.h
// 触摸键盘抽象基类：按键布局与发送按键事件的公共骨架，具体键盘（中文/数字）继承实现。
#ifndef AEA_ABSTRACT_KEYBOARD_H
#define AEA_ABSTRACT_KEYBOARD_H

#include <QApplication>
#include <QWidget>
#include <QAbstractSpinBox>
#include <QLineEdit>
#include <QTime>
#include <QTimeEdit>
#include <QDebug>
#include <QKeyEvent>
#include <QColor>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QPainterPath>
#include <QRegion>
#include <QHash>
#include <QIcon>
#include <QPainter>
#include <QPixmap>

#include "AeaKeyboardStyle.h"
#include "KeyButton.h"

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

/// 时分秒（HH:mm:ss）触屏连续输入过滤器（自 aerocount 沉淀，2026-11-25）：
/// 点击或获焦后默认全选；全选状态下首次键入数字修改最左第 1 位，再次键入自左向右
/// 依次覆盖 6 位（如输入 000005 → 00:00:05），每输入一位自动高亮下一待输入位；
/// 小时十位钳制 0..2、分/秒十位钳制 0..5；退格左退清零，清空/Delete 重置为 00:00:00 并全选。
/// ⚠ 必须同时 installEventFilter 到 QTimeEdit 本体与内部 QLineEdit：QTimeEdit 经
/// focusProxy 代理焦点时 QApplication::focusWidget() 返回 QTimeEdit 本体，只挂内部
/// QLineEdit 会漏拦截触摸键盘发来的 KeyPress（导致全选后键入任何数字均被判无效）。
class DurationInputFilter final : public QObject
{
public:
    DurationInputFilter(QTimeEdit *edit, QLineEdit *lineEdit)
        : QObject(edit)
        , m_edit(edit)
        , m_lineEdit(lineEdit)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (!m_edit || !m_lineEdit)
            return QObject::eventFilter(watched, event);

        // 仅在「获焦」或「点击内嵌文本区」时补一次延迟全选：QTimeEdit 本体上的
        // 鼠标事件（右侧上下微调箭头）必须原样放行，绝不能连带选中文本。
        const bool isFocusIn = (event->type() == QEvent::FocusIn);
        const bool isLineClick = (watched == m_lineEdit)
                                 && (event->type() == QEvent::MouseButtonPress
                                     || event->type() == QEvent::MouseButtonRelease);
        if (isFocusIn || isLineClick) {
            m_freshFocus = true;
            m_cursorPos = 0;
            // 延后到事件循环下一拍全选：避开 QLineEdit 自身鼠标按下/抬帧清选区
            // （QLineEdit 的 mousePress 会按点击位置置光标并清掉选区，同帧内
            // 先 selectAll 会被它反手抹掉，故只能下一拍补）。
            const QPointer<QLineEdit> le(m_lineEdit);
            QTimer::singleShot(0, m_lineEdit, [le]() {
                // focusProxy 形态下焦点归属本体，le->hasFocus() 可能为假，两者取或
                if (le && (le->hasFocus() || le->parentWidget()->hasFocus()))
                    le->selectAll();
            });
            return QObject::eventFilter(watched, event);
        }

        if (event->type() != QEvent::KeyPress)
            return QObject::eventFilter(watched, event);

        const auto *keyEvent = static_cast<QKeyEvent *>(event);
        const int key = keyEvent->key();
        const QString txt = keyEvent->text();

        QChar digitChar;
        if (txt.size() == 1 && txt.at(0).isDigit()) {
            digitChar = txt.at(0);
        } else if (key >= Qt::Key_0 && key <= Qt::Key_9) {
            digitChar = QLatin1Char(char('0' + (key - Qt::Key_0)));
        }

        auto applyBuffer = [this](bool selectAllAfter) {
            const int hh = qBound(0, m_buf.mid(0, 2).toInt(), 23);
            const int mm = qBound(0, m_buf.mid(2, 2).toInt(), 59);
            const int ss = qBound(0, m_buf.mid(4, 2).toInt(), 59);
            m_buf = QStringLiteral("%1%2%3")
                        .arg(hh, 2, 10, QLatin1Char('0'))
                        .arg(mm, 2, 10, QLatin1Char('0'))
                        .arg(ss, 2, 10, QLatin1Char('0'));
            m_edit->setTime(QTime(hh, mm, ss));
            if (selectAllAfter) {
                m_lineEdit->selectAll();
            } else if (m_cursorPos < 6) {
                // "HH:mm:ss" 中第 0..5 位数字对应的字符下标，高亮下一待输入位
                static const int kDigitIndex[6] = {0, 1, 3, 4, 6, 7};
                m_lineEdit->setSelection(kDigitIndex[m_cursorPos], 1);
            } else {
                m_lineEdit->deselect();
                m_lineEdit->setCursorPosition(m_lineEdit->text().size());
            }
        };

        if (!digitChar.isNull()) {
            if (m_freshFocus || m_cursorPos >= 6) {
                m_buf = QStringLiteral("000000");
                m_cursorPos = 0;
                m_freshFocus = false;
            }
            if (m_cursorPos == 0 && digitChar > QLatin1Char('2'))
                digitChar = QLatin1Char('2');
            else if ((m_cursorPos == 2 || m_cursorPos == 4)
                     && digitChar > QLatin1Char('5'))
                digitChar = QLatin1Char('5');
            m_buf[m_cursorPos] = digitChar;
            ++m_cursorPos;
            applyBuffer(false);
            return true;
        }
        if (key == Qt::Key_Backspace) {
            if (m_freshFocus) {
                m_buf = QStringLiteral("000000");
                m_cursorPos = 0;
                m_freshFocus = true;
                applyBuffer(true);
                return true;
            }
            if (m_cursorPos > 0) {
                --m_cursorPos;
                m_buf[m_cursorPos] = QLatin1Char('0');
            }
            applyBuffer(false);
            return true;
        }
        if (key == Qt::Key_Clear || key == Qt::Key_Delete) {
            m_buf = QStringLiteral("000000");
            m_cursorPos = 0;
            m_freshFocus = true;
            applyBuffer(true);
            return true;
        }
        if (key == Qt::Key_Period || txt == QLatin1String(".")) {
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QTimeEdit *m_edit = nullptr;
    QLineEdit *m_lineEdit = nullptr;
    QString m_buf = QStringLiteral("000000");
    int m_cursorPos = 0;
    bool m_freshFocus = true;
};

/// @brief 为 QTimeEdit（HH:mm:ss）启用触屏全选 + 自左向右连续数字输入，并打上 numericInput
///        属性以默认弹出九宫格数字键盘（T9Keyboard::Digits）。幂等，可安全重复调用。
inline void setupDurationEdit(QTimeEdit *edit)
{
    if (!edit)
        return;
    edit->setProperty("numericInput", true);
    if (QLineEdit *lineEdit = edit->findChild<QLineEdit *>()) {
        lineEdit->setProperty("numericInput", true);
        if (!edit->property("aeaDurationFilterInstalled").toBool()) {
            edit->setProperty("aeaDurationFilterInstalled", true);
            auto *filter = new DurationInputFilter(edit, lineEdit);
            edit->installEventFilter(filter);
            lineEdit->installEventFilter(filter);
        }
    }
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

/// 面板圆角半径（px）：三键盘自绘圆角与圆角 mask 的唯一来源，
/// 两处 setMask + paintKeyboardPanel 必须与此一致
constexpr int kCornerRadius = 15;

/// 面板自绘：禁 QSS 之后 `background-color + border-radius + border` 的唯一替代。
/// 品牌青底 + 圆角；withBorder=true 时另描 1px 灰边（26 键全键盘带边、
/// 小键盘不带——这是两者既定差异，非遗漏）。
inline void paintKeyboardPanel(QPainter &p, const QRect &rect, bool withBorder)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addRoundedRect(QRectF(rect), kCornerRadius, kCornerRadius);
    p.fillPath(path, brandPanelColor());
    if (!withBorder)
        return;
    // 0.5px 内缩：1px 描边有一半落在 rect 外会被裁掉，四边会缺一半
    QPainterPath edge;
    edge.addRoundedRect(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5), kCornerRadius, kCornerRadius);
    QPen pen(panelBorderColor());
    pen.setWidthF(1.0);
    p.setPen(pen);
    p.strokePath(edge, pen);
}

class AbstractKeyboard : public QWidget
{
    Q_OBJECT
public:
    AbstractKeyboard(QWidget *parent = nullptr) : QWidget(parent) {
        // 面板底色自绘（paintKeyboardPanel）：这里保留 WA_StyledBackground 只是
        // 让嵌入主窗作子控件时也能填上 palette 底色（描边/圆角之外的边角由
        // 圆角 mask 裁掉，与九宫格同款）；禁 QSS 之后不再有背景规则可依赖
        setAttribute(Qt::WA_StyledBackground, true);
        // 宿主在创建键盘之前下发的键帽像素字号（0 = 未下发，各键盘用自身默认值）
        m_keyCapPixels = globalKeyCapPixels();
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

    /// @brief 键帽像素字号：宿主按自己的字号系数（如 aerocount 的 kFontScale）统一下发，
    ///        三个键盘一并生效。未下发时各键盘用自己的默认值（26px / 九宫格自绘档）。
    ///        懒创建的键盘（九宫格）在创建时读进程级默认值，见 setGlobalKeyCapPixels()；
    ///        已弹出的键盘可逐个调本函数。
    virtual void setKeyCapPixels(int px);
    /// @brief 当前生效的键帽像素字号（0 = 未下发）
    int keyCapPixels() const { return m_keyCapPixels; }

    /// @brief 语言切换后刷新键帽文案：宿主 installTranslator 之后对本类实例调一次
    ///        （键盘是宿主窗口的长寿子控件，不随页面重建）。changeEvent 收到
    ///        LanguageChange 时也会自动触发，两条路都通
    virtual void retranslate() {}

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

protected:
    /// @brief 语言变化事件：自动触发 retranslate()（宿主也可主动调）
    void changeEvent(QEvent *event) override
    {
        if (event->type() == QEvent::LanguageChange)
            retranslate();
        QWidget::changeEvent(event);
    }

    /// @brief 收起时清掉仍处于按下态的键帽
    /*
     * 为什么必须有这一段：键帽的按下态用的是 QAbstractButton 内置的 isDown()，
     * 而「确认」键（小键盘/九宫格都有）的回调里会 clearFocus() 让输入框失焦 →
     * InputManager::focusChanged → hideKeyboards() 把键盘收起——**此时用户手指
     * 还按在确认键上**。隐藏的控件不再接收鼠标事件，那一次 MouseButtonRelease
     * 永远到不了键帽，isDown() 就一直是 true；下次再弹出键盘，那个键就一直
     * 画着按下色（keyCapPressedColor）。
     *
     * 2026-11-16 AeroCount 板上实测：「键盘现在有时会出现点击后页面依然显示在
     * 某个按键高亮（实际上已经没有高亮了）」。"有时"正是因为只有会自隐藏键盘的
     * 那几个键（确认/收起）踩得到，普通键的松手事件正常到达。
     *
     * 放在 hideEvent/showEvent 而不是 InputManager::hideKeyboards()：两个键盘
     * （Keyboard / T9Keyboard）都派生自本类，且它们的
     * hideEvent override 末尾都会调 AbstractKeyboard::hideEvent(e)，所以这两处
     * 覆盖**所有**显隐路径——包括宿主应用自己 hide()/show() 键盘，不只 InputManager。
     *
     * 为什么 show 侧也要清：hide() 对**已隐藏**的控件不再发 HideEvent（实测：
     * 连续两次 hide，第二次不进 hideEvent），只靠 hide 侧存在"键盘本就在隐藏态、
     * 键帽却处于按下态"的漏网窗口；show 侧补一次让"弹出来的键盘永不带卡住的键"
     * 成为不变量，代价是每次弹键遍历一遍键帽（键盘一屏几十枚，可忽略）。
     */
    void hideEvent(QHideEvent *event) override
    {
        releaseStuckKeys();
        QWidget::hideEvent(event);
    }

    void showEvent(QShowEvent *event) override
    {
        releaseStuckKeys();
        QWidget::showEvent(event);
    }

private:
    /// @brief 清掉所有处于按下态的键帽（isDown() 为真才动，见 hideEvent 注释）
    void releaseStuckKeys()
    {
        for (QAbstractButton *key : findChildren<QAbstractButton *>()) {
            if (key->isDown())
                key->setDown(false);
        }
    }

private:
    int m_keyCapPixels = 0;   // 0 = 未下发（用各键盘自身默认字号）
};

// 默认实现：把像素字号下发给所有 KeyButton 键帽（26 键全键盘 / 数字小键盘即此口径；
// 九宫格自绘键帽在 T9Keyboard 里覆盖本函数，另带自己的副标/条目字号）
inline void AbstractKeyboard::setKeyCapPixels(int px)
{
    m_keyCapPixels = px;
    for (KeyButton *button : findChildren<KeyButton *>())
        button->setCapPixels(px);
}

}
#endif // AEA_ABSTRACT_KEYBOARD_H
