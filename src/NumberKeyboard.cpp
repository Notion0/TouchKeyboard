#include "NumberKeyboard.h"
#include "KeyButton.h"
#include <QLabel>
#include <QLineEdit>
#include <QAbstractSpinBox>
#include <QVBoxLayout>

using namespace AeaQt;

typedef QList<KeyButton::Mode> Modes;
typedef QList<Modes> ModesList;

// 退格图标由 keyIcon() 现画（见 AbstractKeyboard.h：目标板 Qt 无 PNG 支持）

// 面板尺寸 672×528（原 330×355；用户 2026-11-17「纯数字的小键盘有点小了，尽量放大一点」）：
// 高 = 标题行 48 + 5 行键帽 96 = 528；宽 = 672 = 屏宽 720 − 48（= main.cpp 的 maxWidth，
// 恰走上限 continue、永不被钳，圆角 mask 保持构造期值）。26 键全键盘模板原尺寸为
// 990×300（Keyboard.ui，旧注释「990x360」有误）。键帽行内等分（Ignored + 零 spacing/
// margins）：行 1-3 每枚 224、行 4（-.0⌫）每枚 168、动作行（清空/确认）每枚 336，
// 触控目标远超 44px 下限。
static const QSize kPanelSize(672, 528);
// 标题行高复用 26 键标题栏口径 AeaQt::kTitleBarHeight（=48）；键帽行高 96 = (528 − 48) / 5
constexpr int kTitleRowHeight = AeaQt::kTitleBarHeight;
constexpr int kKeyRowHeight = 96;
// 高度账：标题行 48 + 5 行键帽 96 = 528，必须与 kPanelSize.height() 对齐
static_assert(kTitleRowHeight + 5 * kKeyRowHeight == 528,
              "NumberKeyboard 面板高度账不等于 528");

NumberKeyboard::NumberKeyboard(QWidget *parent) : AbstractKeyboard(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetNoConstraint);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);


    // 标题栏：WOWA logo（白粗体 15pt 居中，衬青底）
    auto* logo = new QLabel(brandLogoText(), this);
    logo->setFont(QFont(QStringLiteral("Microsoft YaHei"), 15, QFont::Bold));
    // 标题栏白字（禁 QSS 后 color 的唯一替代：palette WindowText，子孙继承）
    AeaQt::applyForegroundColor(logo, AeaQt::titleInkColor());
    logo->setAlignment(Qt::AlignCenter);
    logo->setMinimumSize(120, 0);
    logo->setFixedHeight(kTitleRowHeight);   // 标题行高 48：与 26 键标题栏同高（AeroCount 已清空 logo 文本，纯装饰青带）
    logo->setMaximumWidth(150);              // 宽上限 150 保留
    logo->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    layout->addWidget(logo);

    auto createLayout = [&](ModesList list){
        QHBoxLayout *h = new QHBoxLayout;
        h->setSizeConstraint(QLayout::SetNoConstraint);
        // 零 spacing/margins：Qt 子布局默认约 9px 边距会吃掉行宽，键帽就不足 224/168；
        // 行内多枚键帽按 Ignored 策略等分整行 672（3 枚→224、4 枚→168）
        h->setSpacing(0);
        h->setContentsMargins(0, 0, 0, 0);

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

    // 末行四键 - . 0 ⌫（e9bc6ab 形态）：e1ae3ed 修字典时误把退格删掉、只剩 . 0 -，
    // 输错一位只能整段清空重输（2026-10-26 恢复）。退格 display 传 QIcon，
    // 依赖 KeyButton 构造「仅字符串型空 display 才回落 value」的判空口径
    const QList<Modes> modeListBar4 = {
            {{Qt::Key_Minus, "-"}},
            {{Qt::Key_Period, "."}},
            {{Qt::Key_0, "0"}},
            {{Qt::Key_Backspace, "", keyIcon(KeyIcon::Backspace)}},
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
    // 与键行同口径：零 spacing/margins，两枚动作键等分整行 672（各 336）
    actionRow->setSpacing(0);
    actionRow->setContentsMargins(0, 0, 0, 0);
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

    // 面板与全键盘同款：品牌青底 + 15px 圆角，由 paintEvent 自绘
    // （paintKeyboardPanel，禁 QSS）；不描灰边——小键盘面板更干净

    // 真圆角窗口：QSS 圆角只裁内容不裁窗口，四角露底层白底显成方角——
    // 透明背景 + 圆角 mask 从窗口层裁掉（半径与上面 QSS 的 border-radius 一致）
    setMask(roundedWindowMask(kPanelSize, kCornerRadius));
}

KeyButton *NumberKeyboard::createButton(QList<KeyButton::Mode> modes)
{
    KeyButton *button = new KeyButton(modes, this);
    button->onReponse(this, SLOT(onButtonPressed(const int&, const QString&)));
    button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    return button;
}

void NumberKeyboard::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);
    QPainter p(this);
    // 小键盘面板不描灰边（与 26 键全键盘的既定差异）
    AeaQt::paintKeyboardPanel(p, rect(), false);
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
    // 清空：数值框（spin box）走「全选数值段」，其余输入框直接清空
    if (code == Qt::Key_Clear) {
        QWidget *focus = QApplication::focusWidget();
        // 焦点实际落在 spinbox 的内嵌行编辑（qt_spinbox_lineedit）上，必须沿父链补查
        // 一层；直接 cast focusWidget 永远不中，会落到下面的 QLineEdit 分支走 clear()，
        // 数值随即被 interpret 还原（2026-10-19 现场报："按清空没有全选"）
        auto *spin = qobject_cast<QAbstractSpinBox*>(focus);
        if (!spin && qobject_cast<QLineEdit*>(focus))
            spin = qobject_cast<QAbstractSpinBox*>(focus->parentWidget());
        if (spin) {
            // 全选数值段（suffix 不入选）：下次键入直接覆盖，单位不丢、无空白期
            //（clear() 会把 setSuffix 的单位一起清掉、失焦才恢复）
            spin->selectAll();
        } else if (auto *edit = qobject_cast<QLineEdit*>(focus)) {
            edit->clear();
        }
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
