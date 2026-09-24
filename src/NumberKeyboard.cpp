/*
#Author: Qtjun
#WeChat Official Accounts: qthub_com
#QQ Group: 732271126
#Email: 2088201923@qq.com
*/

#include "NumberKeyboard.h"
#include "KeyButton.h"
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

using namespace AeaQt;

typedef QList<KeyButton::Mode> Modes;
typedef QList<Modes> ModesList;

static const QString s_backspace_icon = ":/touchkeyboard/icons/backspace.png";

// 面板尺寸：全键盘(990x360)的小弟——标题栏 30px + 3列x4行键帽约 110x65 + 底部动作行 65px，
// 触控目标远超 48px 下限
static const QSize kPanelSize(330, 355);

NumberKeyboard::NumberKeyboard(QWidget *parent) : AbstractKeyboard(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetNoConstraint);
    layout->setSpacing(0);
    layout->setMargin(0);


    // 标题栏：WOWA logo（白粗体 15pt 居中，衬青底）
    auto* logo = new QLabel(brandLogoText(), this);
    logo->setFont(QFont(QStringLiteral("Microsoft YaHei"), 15, QFont::Bold));
    logo->setStyleSheet(QStringLiteral("color: rgb(255, 255, 255);"));
    logo->setAlignment(Qt::AlignCenter);
    logo->setMinimumSize(120, 0);
    logo->setMaximumSize(150, 34);
    logo->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    layout->addWidget(logo);

    auto createLayout = [&](ModesList list){
        QHBoxLayout *h = new QHBoxLayout;
        h->setSizeConstraint(QLayout::SetNoConstraint);

        foreach (Modes iter, list) {
            KeyButton *button = createButton(iter);
            h->addWidget(button);
        }

        layout->addLayout(h);
    };

    // 纯数字输入场景：只给 0-9 + 小数点 + 退格，其余字符键一律不给
    const QList<Modes> modeListBar1 = {
            {{Qt::Key_1, "1"}},
            {{Qt::Key_2, "2"}},
            {{Qt::Key_3, "3"}},
    };

    const QList<Modes> modeListBar2 = {
            {{Qt::Key_4, "4"}},
            {{Qt::Key_5, "5"}},
            {{Qt::Key_6, "6"}},
    };

    const QList<Modes> modeListBar3 = {
            {{Qt::Key_7, "7"}},
            {{Qt::Key_8, "8"}},
            {{Qt::Key_9, "9"}},
    };

    const QList<Modes> modeListBar4 = {
            {{Qt::Key_Period, "."}},
            {{Qt::Key_0, "0"}},
            {{Qt::Key_Backspace, "", QIcon(s_backspace_icon)}},
    };

    createLayout(modeListBar1);
    createLayout(modeListBar2);
    createLayout(modeListBar3);
    createLayout(modeListBar4);

    // 底部动作行：清空/确认（对标 PUPSIT 小键盘——见 onButtonPressed 分支）
    const QList<Modes> modeListAction = {
            {{Qt::Key_Clear, "", QString::fromUtf8("清空")}},
            {{Qt::Key_Return, "", QString::fromUtf8("确认")}},
    };
    auto* actionRow = new QHBoxLayout;
    actionRow->setSizeConstraint(QLayout::SetNoConstraint);
    actionRow->setSpacing(0);
    foreach (Modes iter, modeListAction) {
        actionRow->addWidget(createButton(iter));
    }
    layout->addLayout(actionRow);

    this->setLayout(layout);
    this->setFixedSize(kPanelSize);

    // 图标键（退格）的图标尺寸必须显式钉：KeyButton 构造期 width()=0，
    // setDisplayContent 会把 iconSize 算成 0x0 导致图标不渲染；而本面板定尺不再
    // 触发 resizeEvent 兜底。先强制布局激活拿到键帽真实几何，再按一半尺寸设图标。
    layout->activate();
    foreach (KeyButton *button, findChildren<KeyButton *>()) {
        if (!button->icon().isNull() && button->width() > 0)
            button->setIconSize(QSize(button->width() / 2, button->height() / 2));
    }

    // 面板与全键盘同款：品牌青底 + 15px 圆角（与全键盘 #keyBoard 规则一致，
    // 但不描灰边——小键盘面板更干净；键帽由 KeyButton 自带白底黑字 QSS）
    this->setStyleSheet(
        QStringLiteral("background-color: %1; border-radius: 15px;")
            .arg(brandPanelColor().name()));
}

KeyButton *NumberKeyboard::createButton(QList<KeyButton::Mode> modes)
{
    KeyButton *button = new KeyButton(modes, this);
    button->onReponse(this, SLOT(onButtonPressed(const int&, const QString&)));
    button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    return button;
}

void NumberKeyboard::resizeEvent(QResizeEvent *e)
{
    AbstractKeyboard::resizeEvent(e);

    // 图标键（退格）的图标尺寸钉过一次就再也不随布局长，这里随键帽重算
    foreach (KeyButton *button, findChildren<KeyButton *>()) {
        if (!button->icon().isNull())
            button->setIconSize(QSize(button->width() / 2, button->height() / 2));
    }
}

void NumberKeyboard::onButtonPressed(const int &code, const QString &text)
{
    // 清空：直接清空焦点输入框（对标 PUPSIT NumericKeypadPanel::clearTarget）
    if (code == Qt::Key_Clear) {
        if (auto *edit = qobject_cast<QLineEdit*>(QApplication::focusWidget()))
            edit->clear();
        return;
    }
    // 确认：主动失焦——一是触发 editingFinished 让"确定后生效"字段走提交路径，
    // 二是焦点变化经 InputManager::focusChanged 收起键盘（PUPSIT 完成键同语义）
    if (code == Qt::Key_Return) {
        if (auto *w = QApplication::focusWidget())
            w->clearFocus();
        return;
    }
    onKeyPressed(code, text);
}
