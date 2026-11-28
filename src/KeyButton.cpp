#include "KeyButton.h"
#include "AeaKeyboardStyle.h"
#include <QDebug>
#include <QFontMetrics>
#include <QMetaType>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QtGlobal>

using namespace AeaQt;

namespace {
/// @brief QVariant 类型判等的双版本兼容封装。
/// Qt 6 起 QVariant::type() 已废弃（编译告警），但替代品分版本：typeId() 仅 Qt 6
/// 有、userType() 两版皆有。用版本宏守护，保住 Qt 5.15/MSVC 宿主构建——模板声明
/// Qt5/6 双版本通吃，禁直接调用单版本 API。
inline bool isVariantType(const QVariant &v, int metaTypeId)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return v.typeId() == metaTypeId;
#else
    return v.userType() == metaTypeId;
#endif
}
}  // namespace


KeyButton::Type KeyButton::find(const QString &value)
{
    static const QRegularExpression lowerRx(QStringLiteral("^[a-z]$"));
    static const QRegularExpression upperRx(QStringLiteral("^[A-Z]$"));
    if (lowerRx.match(value).hasMatch())
        return KeyButton::LowerCase;

    if (upperRx.match(value).hasMatch())
        return KeyButton::UpperCase;

    return KeyButton::SpecialChar;
}

KeyButton::Mode KeyButton::find(KeyButton::Type type)
{
    // 空 modes 防御（release 下 Q_ASSERT 不生效）：m_modes.first() 越界即 UB，
    // 此处保持现状返回—— Keyboard::switchCapsLock 等会遍历所有 KeyButton
    if (m_modes.isEmpty())
        return m_mode;

    foreach (KeyButton::Mode mode, m_modes) {
        if (mode.type == type)
            return mode;
    }

    return m_modes.first();
}

KeyButton::Mode KeyButton::findNext()
{
    if (m_modes.isEmpty())
        return m_mode;

    for(int i = 0; i < m_modes.count(); i++) {
        KeyButton::Mode mode = m_modes[i];
        if (mode.display == m_mode.display) {
            if (i+1 < m_modes.count())
                return m_modes.at(i+1);
            else
                return m_modes.first();
        }
    }

    return m_modes.first();
}

void KeyButton::setDisplayContent(const QVariant &content)
{
    if (isVariantType(content, QMetaType::QString)) {
        // 直接取 QString：原写法 content.toString().toStdString().data() 先转
        // UTF-8 字节再拼回 QString，纯往返还多一层临时对象生命周期陷阱
        this->setText(content.toString());
    }
    else if (isVariantType(content, QMetaType::QIcon)) {
        const QIcon &icon = content.value<QIcon>();
        this->setIcon(icon);
    }
}

KeyButton::KeyButton(const QList<KeyButton::Mode> modes, QWidget *parent) :
    QPushButton(parent)
{
    Q_ASSERT(!modes.isEmpty());
    this->setFocusPolicy(Qt::NoFocus);
    // 键帽底色/圆角/按下态由 paintEvent 自绘（模板禁 QSS），这里不设样式表
    this->setAutoRepeat(true);

    foreach (Mode mode, modes) {
        if (mode.type == Auto) {
            mode.type = find(mode.value);
        }

        // Qt6 移植修正：Qt5 的 QVariant(空串).isNull()==true 兜底「未传 display
        // 时用 value 当显示文本」；Qt6 该语义变化导致 bar1/2/3 的字母/数字键
        // display 停在空串——键帽全空白（仅显式传 display 的 bar4 有字）。
        // 判空追加 isEmpty 兜底，两种语义下都成立；Caps/退格等空显示键的图标
        // 由 Keyboard::resizeButton 按 key 设置，不受影响
        // ⚠ 判空仅限字符串型 variant：QVariant(QIcon).toString() 同样返回空串，
        // 数字小键盘退格键 display 传的是 QIcon，被一并判空后覆盖成 mode.value
        // （该键 value 为空串）→ 图标在构造期即被冲掉、键帽显示空白。非字符串
        // 显示（QIcon 等）一律原样放行（aerocount 分叉线 2026-09-29 修复，回带）
        if (mode.display.isNull()
            || (isVariantType(mode.display, QMetaType::QString)
                && mode.display.toString().isEmpty())) {
            mode.display = mode.value;
        }

        m_modes.append(mode);
    }

    if (!modes.isEmpty()) {
        m_preMode = m_mode = m_modes.first();
        setDisplayContent(m_mode.display);
    }

    connect(this, SIGNAL(pressed()), this, SLOT(onPressed()));
}

KeyButton::Mode KeyButton::mode()
{
    return m_mode;
}

void KeyButton::onReponse(const QObject *receiverObj, const QString &receiver) {
    connect(this, SIGNAL(pressed(int,QString)), receiverObj, receiver.toStdString().c_str());
}

void KeyButton::switchCapsLock()
{
    if (m_mode.type == SpecialChar)
        return;

    m_preMode = m_mode;
    m_mode = find(m_mode.type == LowerCase ? UpperCase : LowerCase);
    setDisplayContent(m_mode.display);
}

void KeyButton::switchSpecialChar()
{
    if (m_mode.type == SpecialChar) {
        m_mode = m_preMode;
    }
    else {
        m_preMode = m_mode;
        m_mode = find(SpecialChar);
    }

    setDisplayContent(m_mode.display);
}

void KeyButton::switching()
{
    m_mode = findNext();
    setDisplayContent(m_mode.display);
}

void KeyButton::onPressed()
{
    emit pressed(m_mode.key, m_mode.value);
}

void KeyButton::setCapPixels(int px)
{
    if (m_capPixels == px)
        return;
    m_capPixels = px;
    update();
}

int KeyButton::fitTextPixels(const QRectF &cap) const
{
    const int base = m_capPixels > 0 ? m_capPixels : kCapDefaultPixels;
    const int avail = qMax(0, int(cap.width()) - 2 * kCapTextPadding);
    QFont f = font();
    for (int px = base; px > kCapMinPixels; --px) {
        f.setPixelSize(px);
        if (QFontMetrics(f).horizontalAdvance(text()) <= avail)
            return px;
    }
    return kCapMinPixels;
}

void KeyButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 键帽矩形：内缩 kCapMargin（原 QSS margin: 5px）+ 圆角 kCapRadius
    const QRectF cap = QRectF(rect()).adjusted(kCapMargin, kCapMargin, -kCapMargin, -kCapMargin);
    QPainterPath path;
    path.addRoundedRect(cap, kCapRadius, kCapRadius);
    p.fillPath(path, isDown() ? keyCapPressedColor() : keyCapFaceColor());

    // 图标键（如大小写）：只画图标，尺寸按键帽的 60% 取，不依赖外部 iconSize
    if (text().isEmpty() && !icon().isNull()) {
        const QSize sz(qRound(cap.width() * 0.6), qRound(cap.height() * 0.6));
        QRect target(QPoint(0, 0), sz);
        target.moveCenter(QRectF(cap).toRect().center());
        icon().paint(&p, target, Qt::AlignCenter,
                     isEnabled() ? QIcon::Normal : QIcon::Disabled,
                     isDown() ? QIcon::On : QIcon::Off);
        return;
    }

    // 文字键帽：像素字号走漏斗（宿主下发 > 模板默认），放不下就逐级缩
    QFont f = font();
    f.setPixelSize(fitTextPixels(cap));
    p.setFont(f);
    p.setPen(isEnabled() ? keyCapInkColor() : keyCapDisabledInkColor());
    p.drawText(cap, Qt::AlignCenter, text());
}
