#include "KeyButton.h"
#include <QDebug>
#include <QRegularExpression>

using namespace AeaQt;


const QString DEFAULT_STYLE_SHEET = "AeaQt--KeyButton { background: white; border-radius: 5px;" \
    "margin: 5px;" \
    "font-size: 26px; color: black;}" \
    "AeaQt--KeyButton:pressed { background: #01ddfd }";

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
    foreach (KeyButton::Mode mode, m_modes) {
        if (mode.type == type)
            return mode;
    }

    return m_modes.first();
}

KeyButton::Mode KeyButton::findNext()
{
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
    if (content.type() == QVariant::String) {
        const QString &text = content.toString().toStdString().data();
        this->setText(text);
    }
    else if (content.type() == QVariant::Icon) {
        const QIcon &icon = content.value<QIcon>();
        this->setIcon(icon);
        this->setIconSize(QSize(1.2 * width(), 1.2 * height()));
    }
}

KeyButton::KeyButton(const QList<KeyButton::Mode> modes, QWidget *parent) :
    QPushButton(parent)
{
    Q_ASSERT(!modes.isEmpty());
    this->setFocusPolicy(Qt::NoFocus);
    this->setStyleSheet(DEFAULT_STYLE_SHEET);
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
        if (mode.display.isNull() || mode.display.toString().isEmpty())
            mode.display = mode.value;

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
