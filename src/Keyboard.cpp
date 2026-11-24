#include "Keyboard.h"
#include "KeyButton.h"
#include "AeaKeyboardStyle.h"
#include <QVBoxLayout>
#include <QFile>
#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
#include <QStringConverter>
#endif
#include <QApplication>
#include <QPushButton>
#include <QLineEdit>
#include <QAbstractSpinBox>
#include <QTextEdit>
#include <QScroller>
#include <QScrollerProperties>
#include <QStyledItemDelegate>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <QRegularExpression>
#include <QDebug>
#include "ui_Keyboard.h"
#include "TouchKbdDict.h"


using namespace AeaQt;

typedef QList<KeyButton::Mode> Modes;
typedef QList<Modes> ModesList;

// 键帽图标改由 keyIcon() 现画（AbstractKeyboard.h）：目标板 Qt 无 PNG 支持，原 png 读出空图

// 三字典路径宏（TOUCHKBD_*）与打开兜底 openTouchKbdDict 收在 TouchKbdDict.h，
// 与九宫格 T9PinyinEngine 共用一处

// 候选条目几何：条目高 36px 落进 40px 候选区视口（标题栏内 candidateContainer），
// 条目宽按实测文字宽 + 左右留白算，最小 50px
constexpr int kCandidateItemHeight = 36;
constexpr int kCandidateItemMinWidth = 50;
constexpr int kCandidateItemPadding = 12;
// 候选条目默认像素字号（原 QSS 时代的 setPointSize(18) ≈ 24px；宿主可按字号系数下发）
constexpr int kCandidateFontPixels = 24;

// 26 键面板左右留白：frame 的 QVBoxLayout 左右各 4px，行容器填满 frame
constexpr int kRowSideMargin = 8;
// 第三行（大小写 + 7 字母 + 退格）非统一键的 stretch 份数
constexpr int kRow3CapsStretch = 70;
constexpr int kRow3LetterStretch = 69;
const double BUTTON_WIDTH_RATIO   = 0.09;
// 底行非统一键的 stretch 份数（各键一份；确认/清空两枚统一宽功能键的份数由
// applyRowStretches 按 kEnterSpec/kClearSpec 反解，写回成员 m_row4TotalStretch）
const int kModeSwitchStretch = 12;  // 「?123」「abc」
const int kLayoutKeyStretch = 20;   // 「九宫格」
const int kLangKeyStretch   = 20;   // 「en/汉」「汉/en」
const int kSpaceStretch     = 26;   // 「空格」
const int kRow4OtherStretch = kModeSwitchStretch + kLayoutKeyStretch
                              + kLangKeyStretch + kSpaceStretch;
const double BUTTON_HEIGHT_RATIO  = 0.2;

const QList<Modes> modeListBar1 = {
    {{Qt::Key_Q, "q"}, {Qt::Key_Q, "Q"}, {Qt::Key_1, "1"}},
    {{Qt::Key_W, "w"}, {Qt::Key_W, "W"}, {Qt::Key_2, "2"}},
    {{Qt::Key_E, "e"}, {Qt::Key_E, "E"}, {Qt::Key_3, "3"}},
    {{Qt::Key_R, "r"}, {Qt::Key_R, "R"}, {Qt::Key_4, "4"}},
    {{Qt::Key_T, "t"}, {Qt::Key_T, "T"}, {Qt::Key_5, "5"}},
    {{Qt::Key_Y, "y"}, {Qt::Key_Y, "Y"}, {Qt::Key_6, "6"}},
    {{Qt::Key_U, "u"}, {Qt::Key_U, "U"}, {Qt::Key_7, "7"}},
    {{Qt::Key_I, "i"}, {Qt::Key_I, "I"}, {Qt::Key_8, "8"}},
    {{Qt::Key_O, "o"}, {Qt::Key_O, "O"}, {Qt::Key_9, "9"}},
    {{Qt::Key_P, "p"}, {Qt::Key_P, "P"}, {Qt::Key_0, "0"}},
    };

const QList<Modes> modeListBar2 = {
    {{Qt::Key_A, "a"}, {Qt::Key_A, "A"}, {Qt::Key_unknown, "."}},
    {{Qt::Key_S, "s"}, {Qt::Key_S, "S"}, {Qt::Key_unknown, "?"}},
    {{Qt::Key_D, "d"}, {Qt::Key_D, "D"}, {Qt::Key_At,      "!"}},
    {{Qt::Key_F, "f"}, {Qt::Key_F, "F"}, {Qt::Key_NumberSign, "*"}},
    {{Qt::Key_G, "g"}, {Qt::Key_G, "G"}, {Qt::Key_Percent, "#"}},
    {{Qt::Key_H, "h"}, {Qt::Key_H, "H"}, {Qt::Key_unknown, "\""}},
    {{Qt::Key_J, "j"}, {Qt::Key_J, "J"}, {Qt::Key_unknown, "&", "&&"}},
    {{Qt::Key_K, "k"}, {Qt::Key_K, "K"}, {Qt::Key_unknown, "%"}},
    {{Qt::Key_L, "l"}, {Qt::Key_L, "L"}, {Qt::Key_unknown, "@"}},
    };

const QList<Modes> modeListBar3 = {
    {{Qt::Key_CapsLock, "", ""/*大小写切换*/}},
    {{Qt::Key_Z, "z"}, {Qt::Key_Z, "Z"}, {Qt::Key_ParenLeft, "("}},
    {{Qt::Key_X, "x"}, {Qt::Key_X, "X"}, {Qt::Key_ParenLeft, ")"}},
    {{Qt::Key_C, "c"}, {Qt::Key_C, "C"}, {Qt::Key_Minus,     "-"}},
    {{Qt::Key_V, "v"}, {Qt::Key_V, "V"}, {Qt::Key_unknown,   "_"}},
    {{Qt::Key_B, "b"}, {Qt::Key_B, "B"}, {Qt::Key_unknown,   ":"}},
    {{Qt::Key_N, "n"}, {Qt::Key_N, "N"}, {Qt::Key_Semicolon, ";"}},
    {{Qt::Key_M, "m"}, {Qt::Key_M, "M"}, {Qt::Key_Slash,     "/"}},
    // 退格键帽文案「退格」由 applyKeyLabels() 统一下发（tr() + 统一规格表 kBackspaceSpec），
    // 不写死在 modes 里——写死就绕过了 tr()，切语言时不会跟着变
    {{Qt::Key_Backspace, "", ""/*退格*/}}
};

const QList<Modes> modeListBar4 = {
    {{Qt::Key_Mode_switch, "",  "?123"}},
    // 布局切换键：点击切到九宫格 T9Keyboard（InputManager 接管切换，见 layoutSwitchRequested）。
    // 放在中/英键左边——中/英键须保持「空格左邻」这一既定位置（README 中/英状态标识节）
    {{Qt::Key_Context2,    "",  ""/*九宫格*/}},
    // 中/英切换键（空格左邻，兼作中英态状态标识）：display 两态循环「en」/「汉」，
    // 大字键帽样式见 fitKeyFont（objectName=langSwitchKey）；两态都由 m_isChinese 驱动
    {{Qt::Key_Context1,    "",  "en"},    {Qt::Key_Context1, "", QStringLiteral("汉")}},
    {{Qt::Key_Space,       " ", ""/*空格*/}},
    {{Qt::Key_Enter,       "",  ""/*确认*/}},
    // 清空折进底行（与 PUPSIT 全键盘 Clear+Enter 同形态；确认不单列——回车即确认，
    // 二者行为完全相同，单列属冗余。数字小键盘无回车键，其底行才保留清空/确认）
    {{Qt::Key_Clear,       "", ""/*清空*/}},
};

Keyboard::Keyboard(QWidget *parent) :
    AbstractKeyboard(parent),
    ui(new Ui::Keyboard),
    m_isChinese(false),
    m_englishOnly(false)
{
    ui->setupUi(this);

    // 面板底色/圆角/灰边由 paintEvent 自绘（paintKeyboardPanel，禁 QSS），
    // .ui 里写死的青底只作 Designer 预览；logo 文本按品牌常量套用
    if (ui->labPY) {
        ui->labPY->setText(brandLogoText());
        // 标题栏白字（原 QSS color: rgb(255,255,255)）：palette 唯一替代
        applyForegroundColor(ui->labPY, titleInkColor());
    }

    // 真圆角窗口：QSS 圆角只裁内容不裁窗口，四角会露底层显成方角——
    // 圆角 mask 从窗口层裁掉（半径与 paintKeyboardPanel 的圆角一致）
    setMask(roundedWindowMask(size(), kCornerRadius));

    // 创建中文候选控件（如果需要）
    m_chineseWidget = new ChineseWidget(ui->candidateContainer);
    // 将候选控件添加到 candidateContainer 的布局中（如果容器有布局的话）
    if (ui->candidateContainer->layout()) {
        ui->candidateContainer->layout()->addWidget(m_chineseWidget);
    } else {
        QHBoxLayout *layout = new QHBoxLayout(ui->candidateContainer);
        layout->setContentsMargins(0,0,0,0);
        layout->addWidget(m_chineseWidget);
    }

    // 连接信号
    connect(m_chineseWidget, &AeaQt::ChineseWidget::pressedChanged,
            this, &Keyboard::onCandidateCommitted);
    connect(m_chineseWidget, &AeaQt::ChineseWidget::pressedChanged,
            this, &Keyboard::clearBufferText);

    // 获取第一行容器并添加按钮
    QHBoxLayout *row1Layout = qobject_cast<QHBoxLayout*>(ui->row1Container->layout());
    if (row1Layout) {
        for (const auto &modes : modeListBar1) {
            KeyButton *button = createButton(modes);
            row1Layout->addWidget(button);
        }
    }

    // 第二行
    QHBoxLayout *row2Layout = qobject_cast<QHBoxLayout*>(ui->row2Container->layout());
    if (row2Layout) {
        for (const auto &modes : modeListBar2) {
            KeyButton *button = createButton(modes);
            row2Layout->addWidget(button);
        }
    }

    // 第三行（需要处理拉伸因子）
    m_row3Layout = qobject_cast<QHBoxLayout*>(ui->row3Container->layout());
    if (m_row3Layout) {
        for (int i = 0; i < modeListBar3.count(); ++i) {
            KeyButton *button = createButton(modeListBar3.at(i));
            if (i == 0 || i == modeListBar3.count() - 1) {
                // 第一个和最后一个按钮拉伸因子 70
                m_row3Layout->addWidget(button, kRow3CapsStretch);
            } else {
                m_row3Layout->addWidget(button, kRow3LetterStretch);
            }
        }
    }

    // 第四行（不同按钮不同拉伸因子）
    m_row4Layout = qobject_cast<QHBoxLayout*>(ui->row4Container->layout());
    if (m_row4Layout) {
        for (int i = 0; i < modeListBar4.count(); ++i) {
            KeyButton *button = createButton(modeListBar4.at(i));
            // 中/英切换键（空格左邻）兼作中英态状态标识：键帽「汉/en」「en/汉」
            // 用加粗大字，明显大于普通键帽。现场要求把状态显示从标题栏挪到这个键位——
            // 它是全键盘上唯一「与中文相关且常驻可见」的键，且本来就在空格左边，
            // 合并后功能不丢（该键此前只有 en/汉 切换一个功能）。
            // 实现途径：setFont + fitKeyFont 按实际键宽缩字（模板禁 QSS 之后不再有
            // ID 选择器样式表，字号一律走字体对象），objectName 保留给单测/查找
            // 字号与键宽经现场反馈校正：36px 单字放得下，但"汉/en"五字符组合按
            // 36px 算约 86px 超出键位；现基准 30px（组合约 67px）+ 20 份 stretch，
            // 再由 resizeButton → fitKeyFont 按实际可用宽兜底缩字，更窄的宿主也不截断
            // 按键码取 stretch（不按下标：底行插过布局切换键，下标会漂）
            int stretch = 22;    // 回车与清空等宽（占位，applyRowStretches 会按规格表重算）
            switch (button->mode().key) {
            case Qt::Key_Mode_switch: stretch = kModeSwitchStretch; break;
            // 2026-10-26 aerocount 板上反馈「九宫格」「en/汉」显示不全：宿主把键盘从 990
            // 钳到 672 宽后，14 份只剩约 77px、扣 margin 后约 67px，放不下 26px「九宫格」
            // （约 78px）与 30px 粗体「en/汉」。两键各提到 20 份（672 宽下约 100px 可用），
            // 空格 38→26 让出；再由 resizeButton 按实际可用宽兜底缩字（fitKeyFont），
            // 更窄的宿主也不截断
            case Qt::Key_Context2:    stretch = kLayoutKeyStretch; break;
            case Qt::Key_Context1:    stretch = kLangKeyStretch; break;
            case Qt::Key_Space:       stretch = kSpaceStretch; break;
            default: break;
            }
            m_row4Layout->addWidget(button, stretch);
        }
    }

    // 标题栏按钮设置（保持原有）
    QPushButton *closeBtn = ui->btnClose;
    if (closeBtn) {
        closeBtn->setFocusPolicy(Qt::NoFocus);
        // 裸图标钮：flat 之后 Fusion 不再画按钮底与边框（禁 QSS 的唯一替代），
        // 图标由 keyIcon(KeyIcon::Hide) 现画，命中区 44×44（kCloseButtonSize）
        closeBtn->setFlat(true);
        connect(closeBtn, &QPushButton::clicked, this, &Keyboard::hide);
    }

    // 可选：设置标题栏其他按钮焦点策略
    QList<QWidget*> titleWidgets = ui->titleBar->findChildren<QWidget*>();
    foreach (QWidget *w, titleWidgets) {
        w->setFocusPolicy(Qt::NoFocus);
    }

    // 顺序不能反：先按统一规格表重算底两行 stretch（fitKeyFont 要用总份数算可用宽），
    // 再调 resizeButton 设图标/缩字，最后统一下发 tr() 键帽文案
    applyRowStretches();
    resizeButton();
    applyKeyLabels();
    updateCloseButton();

    // 初始中英态为英文（m_isChinese=false）：候选区收起、中英键帽停在
    // m_modes.first() 的「en」，两处一致，故只需刷一次候选区可见性
    updateLangState();
}

// ========== 新增析构函数，释放 UI 对象 ==========
Keyboard::~Keyboard()
{
    delete ui;
}
// =============================================

void Keyboard::resizeEvent(QResizeEvent *e)
{
    // 顺序不能反：stretch 先按新宽度重算，resizeButton 的缩字估宽才准
    applyRowStretches();
    resizeButton();
    // 圆角 mask 随尺寸重算；判重防 setMask 触发二次 resize 时反复重入
    const QRegion m = roundedWindowMask(size(), kCornerRadius);
    if (mask() != m)
        setMask(m);
}

void Keyboard::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);
    QPainter p(this);
    // 26 键面板带 1px 灰边（小键盘不带——既定差异）
    paintKeyboardPanel(p, rect(), true);
}

// ---------------------------------------------------------------------------
// W7 功能键统一规格表落地（AeaKeyboardStyle.h）
// ---------------------------------------------------------------------------

/// 底两行 stretch 与总份数：功能键统一规格表的基准宽换算而来。
/// 第三行两端（大小写/退格）与第四行的功能键按 kBackspaceSpec/kEnterSpec/kClearSpec
/// 的基准宽反解 stretch 份数，其余键份数不变；总份数写回 m_row4TotalStretch
/// （fitKeyFont 的可用宽估算要用它）。宿主把键盘钳到 672 宽时，统一宽放不下就
/// 退回与其余键同份（stretchForUnifiedWidth 内部兜底），不会出现 0/负份。
void Keyboard::applyRowStretches()
{
    if (m_row3Layout) {
        // 第三行：大小写 + 7 字母 + 退格；退格取统一基准宽
        const int backspaceStretch = AeaQt::stretchForUnifiedWidth(
            AeaQt::kBackspaceSpec.baseWidth, 1, kRow3CapsStretch + 7 * kRow3LetterStretch,
            width());
        for (int i = 0; i < m_row3Layout->count(); ++i) {
            QWidget *w = m_row3Layout->itemAt(i)->widget();
            if (!w)
                continue;
            KeyButton *button = qobject_cast<KeyButton *>(w);
            if (!button)
                continue;
            const int stretch = button->mode().key == Qt::Key_Backspace
                ? backspaceStretch
                : (button->mode().key == Qt::Key_CapsLock ? kRow3CapsStretch
                                                          : kRow3LetterStretch);
            m_row3Layout->setStretch(i, stretch);
        }
    }

    if (m_row4Layout) {
        // 第四行：?123 + 九宫格 + 中英 + 空格 + 确认 + 清空；确认/清空取统一基准宽
        const int other = kRow4OtherStretch;
        const int unified = AeaQt::stretchForUnifiedWidth(
            AeaQt::kEnterSpec.baseWidth, 2, other, width());
        int total = other + 2 * unified;
        for (int i = 0; i < m_row4Layout->count(); ++i) {
            QWidget *w = m_row4Layout->itemAt(i)->widget();
            if (!w)
                continue;
            KeyButton *button = qobject_cast<KeyButton *>(w);
            if (!button)
                continue;
            int stretch;
            switch (button->mode().key) {
            case Qt::Key_Enter:       stretch = unified; break;   // 「确认」
            case Qt::Key_Clear:       stretch = unified; break;   // 「清空」
            case Qt::Key_Mode_switch: stretch = kModeSwitchStretch; break;
            case Qt::Key_Context2:    stretch = kLayoutKeyStretch; break;
            case Qt::Key_Context1:    stretch = kLangKeyStretch; break;
            case Qt::Key_Space:       stretch = kSpaceStretch; break;
            default:                  stretch = kRow3LetterStretch; break;
            }
            m_row4Layout->setStretch(i, stretch);
            total += stretch;
        }
        // 上面累加时 other 已含 Mode_switch/Layout/Lang/Space 四项，default 分支的
        // 字母键不在这行；重算一遍总份数，避免把 other 重复计入
        total = 0;
        for (int i = 0; i < m_row4Layout->count(); ++i) {
            QLayoutItem *item = m_row4Layout->itemAt(i);
            if (item && item->widget())
                total += m_row4Layout->stretch(i);
        }
        m_row4TotalStretch = qMax(1, total);
    }
}

/// 键帽文案（tr()）单一出处：构造、切符号页、语言切换都走这里。
/// modes 里的 display 只作兜底占位（见 modeListBar3/4 的空串注释），
/// 真正的可见文案由本函数按统一规格表下发——写死在 modes 里会绕过 tr()，
/// 切语言时不会跟着变。
void Keyboard::applyKeyLabels()
{
    const auto label = [](KeyButton *button, const QString &text) {
        if (button && !text.isEmpty())
            button->setText(text);
    };
    foreach (KeyButton *button, findChildren<KeyButton *>()) {
        switch (button->mode().key) {
        case Qt::Key_Backspace: label(button, QObject::tr("退格")); break;
        case Qt::Key_Enter:     label(button, QObject::tr("确认")); break;
        case Qt::Key_Clear:     label(button, QObject::tr("清空")); break;
        case Qt::Key_Space:     label(button, QObject::tr("空格")); break;
        case Qt::Key_Context2:  label(button, QObject::tr("九宫格")); break;
        default: break;
        }
    }
}

/// 标题栏收起钮：flat + 现画图标 + 44 命中区（kCloseButtonSize）。
/// .ui 里 btnClose 的 maximumSize 仍是 30×30、标题栏 40 高——那两项在 Designer
/// 里改不动运行时，故运行时按规格表强制下发；.ui 同步改（见 Keyboard.ui）。
void Keyboard::updateCloseButton()
{
    QPushButton *closeBtn = ui->btnClose;
    if (!closeBtn)
        return;
    closeBtn->setFlat(true);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setMinimumSize(AeaQt::kCloseButtonSize, AeaQt::kCloseButtonSize);
    closeBtn->setMaximumSize(AeaQt::kCloseButtonSize, AeaQt::kCloseButtonSize);
    closeBtn->setIcon(keyIcon(KeyIcon::Hide));
    closeBtn->setIconSize(QSize(AeaQt::kCloseButtonSize - 12,
                                AeaQt::kCloseButtonSize - 12));
    closeBtn->setProperty("tkIconSet", true);
}

/// 语言切换后刷新键帽文案：宿主 installTranslator 之后对键盘实例调一次；
/// AbstractKeyboard::changeEvent 也会转到这里（键盘是长寿子控件，
/// rebuildPagesForLanguage 重建页面时它不重建，必须靠 changeEvent 兜住）。
void Keyboard::retranslate()
{
    applyKeyLabels();
    // 标题栏 logo/品牌文本同样可能带 tr（宿主按品牌常量覆写，见构造函数）
    if (ui->labPY)
        ui->labPY->setText(brandLogoText());
}

/// 键帽像素字号下发（宿主按字号系数统一下发；0 = 用模板默认值）。
/// KeyButton 键帽与候选条目一并走，两个出口都收在 AeaKeyboardStyle 的漏斗里。
void Keyboard::setKeyCapPixels(int px)
{
    const int capPx = px > 0 ? px : AeaQt::globalKeyCapPixels();
    const int effective = capPx > 0 ? capPx : AeaQt::kCapDefaultPixels;
    foreach (KeyButton *button, findChildren<KeyButton *>()) {
        QFont f = button->font();
        f.setPixelSize(effective);
        button->setFont(f);
    }
    if (m_chineseWidget)
        m_chineseWidget->setCapPixels(effective);
    resizeButton();
}

void Keyboard::hideEvent(QHideEvent *e)
{
    // 收起即丢弃未提交拼音：拼音缓冲挂在 Keyboard 实例上（跨输入框存活），
    // 不清理时切到下一个输入框，候选栏还留着上一个输入框的拼音结果、
    // buffer 也会接着旧串往下拼（中文态输入框切换的串台）
    clearBufferText();
    if (m_chineseWidget)
        m_chineseWidget->clear();
    AbstractKeyboard::hideEvent(e);
}

void Keyboard::switchCapsLock()
{
    QList<KeyButton *> buttons = findChildren<KeyButton *>();
    foreach(KeyButton *button, buttons)
        button->switchCapsLock();
}

void Keyboard::switchSpecialChar()
{
    QList<KeyButton *> buttons = findChildren<KeyButton *>();
    foreach(KeyButton *button, buttons) {
        // 中/英切换键必须排除：它的两个形态 value 都是空串，KeyButton::find() 一律
        // 归入 SpecialChar 类型，于是 KeyButton::switchSpecialChar 会把 m_mode 打回
        // m_preMode——切一次符号页，键帽从「汉」翻回「en」而 m_isChinese 仍为 true
        // （状态显示与实际中英态相反）；再点一次切换键则因 findNext 从「en」接着
        // 翻成「汉」，彻底颠倒。该键现在是状态标识，错一次用户就看错状态，故在此挡住
        if (button->mode().key == Qt::Key_Context1)
            continue;
        // 模式切换键（左下角）自身也排除：它只有一个 mode 且类型被判为 SpecialChar，
        // KeyButton::switchSpecialChar 对它执行 m_mode = m_preMode 等于不变，键帽
        // 永远停在「?123」——切进符号页后用户看不出按它能回去。其键帽由本函数
        // 末尾按 m_isSpecialChar 统一设置（与中英键同一思路：键帽表达目标态）
        if (button->mode().key == Qt::Key_Mode_switch)
            continue;
        button->switchSpecialChar();
    }

    m_isSpecialChar = !m_isSpecialChar;

    // 左下角模式切换键键帽表达"当前页/按下去去哪"：符号页显示「abc」（按它回字母页），
    // 字母页显示「?123」（按它去符号页）。现场反馈：原来切过去后还是「?123」，
    // 操作员不知道这个键此时是"返回字母"用的
    for (KeyButton *button : buttons) {
        if (button->mode().key == Qt::Key_Mode_switch) {
            button->setText(m_isSpecialChar ? QStringLiteral("abc")
                                            : QStringLiteral("?123"));
        }
    }

    // 功能键键帽（退格/确认/清空/空格）走 applyKeyLabels 重下发：上面那轮
    // switchSpecialChar 会把 m_mode.display 回写成 modes 里的空串，不补这一次，
    // 这几枚键的 tr() 文案就被抹成空白
    applyKeyLabels();
}

void Keyboard::setEnglishOnly(bool enabled)
{
    if (m_englishOnly == enabled)
        return;

    // 先切回英文再上锁：顺序反了会被下面 switchEnOrCh 的英文-only 守卫挡回来，
    // 键盘将停在中文态（候选区开着，用户照样能点候选却上不了屏）
    if (enabled && m_isChinese)
        switchEnOrCh();

    m_englishOnly = enabled;
    updateLangState();
}

bool Keyboard::isChineseMode() const
{
    return m_isChinese;
}

void Keyboard::setChineseMode(bool chinese)
{
    // switchEnOrCh 自带英文-only 守卫：英文-only 下请求中文被忽略
    if (m_isChinese != chinese)
        switchEnOrCh();
}

void Keyboard::switchEnOrCh()
{
    // 英文-only 字段（密码类）不接受切到中文：候选能点选却过不了字段 validator，
    // 会表现为"中文能显示但不能选中写入"。直接吞掉这次切换
    if (m_englishOnly)
        return;

    m_isChinese = !m_isChinese;

    if (! m_isChinese) {
        // 离开中文态：丢弃未提交的拼音缓冲与候选残留——否则切回中文时候选栏
        // 还挂着上一串拼音的结果， buffer 也会接着旧串往下拼
        clearBufferText();
        m_chineseWidget->clear();
    }

    updateLangState();
    // 中/英键帽文本（汉/en、en/汉 双态组合）已由 updateLangState 按 m_isChinese
    // 统一设置——状态显示与候选区可见性同一出处，不在此重复驱动
}

void Keyboard::updateLangState()
{
    // 候选区仅中文态常驻：英文态收起（容器高度随之归 0，不占标题栏空间）
    ui->candidateContainer->setVisible(m_isChinese);

    // 中/英切换键（空格左邻）键帽文本同时表达"当前态/目标态"：中文态「汉/en」、
    // 英文态「en/汉」——操作员不看教程也知道当前是什么、点了会变成什么（现场要求）。
    // 文本完全由 m_isChinese 驱动：不走 KeyButton::switching 的 modes 循环，
    // 循环态只有单个字、表达不了组合语义；点击行为不变（仍发 Context1 键码）
    for (KeyButton *button : findChildren<KeyButton *>()) {
        if (button->mode().key == Qt::Key_Context1) {
            button->setText(m_isChinese ? QStringLiteral("汉/en")
                                        : QStringLiteral("en/汉"));
            // 英文-only 时置灰切换键：看得见但点不动，比"能点却毫无反应"明确
            button->setEnabled(!m_englishOnly);
        }
    }
}

void Keyboard::onCandidateCommitted(const int &code, const QString &text)
{
    // 候选上屏：焦点控件直接插入优先，其余控件兜底合成键事件。取此路径是与
    // 旧版生产键盘（frmInput::insertValue，Qt5 线长期在役）拉齐——原实现把候选
    // 也走 onKeyPressed(-1, text) 的合成事件，而 -1 不是合法 Qt::Key，焦点控件
    // 受理与否没有保证；QLineEdit/QTextEdit 直接插入则与字母键落屏同一套插入
    // 逻辑（光标位置/选区替换/验证器语义一致）
    if (auto *edit = qobject_cast<QLineEdit*>(QApplication::focusWidget()))
        edit->insert(text);
    else if (auto *te = qobject_cast<QTextEdit*>(QApplication::focusWidget()))
        te->insertPlainText(text);
    else
        onKeyPressed(code, text);
}

void Keyboard::onButtonPressed(const int &code, const QString &text)
{
    // 布局切换（→ 九宫格）：只发请求，由 InputManager 收起本键盘、弹九宫格
    if (code == Qt::Key_Context2) {
        emit layoutSwitchRequested();
        return;
    }

    if (code == Qt::Key_Enter || code == Qt::Key_Return) {
        QWidget *w = QApplication::focusWidget();
        if (w) {
            // 提交 = 主动失焦（与数字小键盘「确认」键同一路径）：QLineEdit 对
            // 「改过的文本」失焦会自行发一次 editingFinished——Qt5.15 实测证实
            // （同时实测：另加手动 emit lineEdit->editingFinished() 会与失焦
            // 路径叠加成双发，宿主「确定后生效」字段被触发两遍，故只走失焦）
            w->clearFocus();
        }
        // 收起键盘
        this->hide();
        return;
    }
    // 清空：清拼音缓冲 + 数值框全选数值段 / 其余输入框清空（与数字小键盘同语义）
    if (code == Qt::Key_Clear) {
        clearBufferText();
        QWidget *focus = QApplication::focusWidget();
        // 焦点实际落在 spinbox 的内嵌行编辑（qt_spinbox_lineedit）上，必须沿父链补查
        // 一层；直接 cast focusWidget 永远不中，会落到下面的 QLineEdit 分支走 clear()，
        // 数值随即被 interpret 还原（2026-10-19 现场报："按清空没有全选"）
        auto *spin = qobject_cast<QAbstractSpinBox*>(focus);
        if (!spin && qobject_cast<QLineEdit*>(focus))
            spin = qobject_cast<QAbstractSpinBox*>(focus->parentWidget());
        if (spin) {
            // 全选数值段（suffix 不入选）：下次键入直接覆盖，单位不丢、无空白期
            spin->selectAll();
        } else if (auto *edit = qobject_cast<QLineEdit*>(focus)) {
            edit->clear();
        } else if (auto *te = qobject_cast<QTextEdit*>(focus)) {
            te->clear();
        }
        return;
    }
    if (! m_isChinese) {
        onKeyPressed(code, text);
        m_bufferText.clear();
        return;
    }

    const QRegularExpression rx(QStringLiteral("^[a-zA-Z]$"));
    if (!rx.match(text).hasMatch() && m_bufferText.isEmpty()) {
        onKeyPressed(code, text);
        return;
    }

    if (code == Qt::Key_Backspace)
        m_bufferText.chop(1);
    else
        // 拼音一律小写：键帽 text 随大小写锁定给大写，字典全小写——不转小写
        // 时开着大写锁定拼拼音会一个候选都不出（数字/符号不受 toLower 影响）
        m_bufferText.append(text.toLower());
    m_chineseWidget->setText(m_bufferText);
}

void Keyboard::clearBufferText()
{
    m_bufferText.clear();
}

KeyButton *Keyboard::createButton(QList<KeyButton::Mode> modes)
{
    KeyButton *button = new KeyButton(modes, this);
    button->onReponse(this, SLOT(onButtonPressed(const int&, const QString&)));
    button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    return button;
}

/// 按底行实际可用宽把键帽字号缩到放得下（不超过 basePx）。
/// 禁 QSS 之后字号一律走字体对象（原 ID 选择器样式表是 QSS 时代的产物）；
/// fitKeyFont 按实际键宽下发，放不下才缩，更窄的宿主也不截断。
/// 可用宽 = 键盘宽 × 份数/总份数 − KeyButton 的 margin 5px×2 − 左右留白 4px×2。
void Keyboard::fitKeyFont(KeyButton *button, int basePx, bool bold, int stretch)
{
    const int avail = width() * stretch / qMax(1, m_row4TotalStretch) - 18;
    if (avail <= 0)
        return;
    // 中英键两态文本都量（「en/汉」「汉/en」），取较宽者
    QStringList texts{button->text()};
    if (button->mode().key == Qt::Key_Context1)
        texts << QStringLiteral("en/汉") << QStringLiteral("汉/en");
    int px = basePx;
    for (; px > AeaQt::kCapMinPixels; --px) {
        QFont f = button->font();
        f.setPixelSize(px);
        f.setBold(bold);
        const QFontMetrics fm(f);
        int widest = 0;
        for (const QString &t : texts)
            widest = qMax(widest, fm.horizontalAdvance(t));
        if (widest <= avail)
            break;
    }
    const QString name = button->mode().key == Qt::Key_Context1
            ? QStringLiteral("langSwitchKey") : QStringLiteral("layoutSwitchKey");
    button->setObjectName(name);
    QFont f = button->font();
    f.setPixelSize(px);
    f.setBold(bold);
    button->setFont(f);
}

void Keyboard::resizeButton()
{
    // 标题栏收起键：禁 QSS 后 flat + 现画图标（keyIcon），命中区与图标尺寸由
    // updateCloseButton() 按 kCloseButtonSize 统一下发（≥44 触控下限）
    if (QPushButton *closeBtn = ui->btnClose) {
        if (!closeBtn->property("tkIconSet").toBool()) {
            closeBtn->setFlat(true);
            closeBtn->setIcon(keyIcon(KeyIcon::Hide));
            closeBtn->setProperty("tkIconSet", true);
        }
        const int side = qMin(closeBtn->width(), closeBtn->height());
        if (side > 0)
            closeBtn->setIconSize(QSize(side, side));
    }

    foreach (KeyButton *button, findChildren<KeyButton *>()) {
        int fixedWidth = width()*BUTTON_WIDTH_RATIO;
        int fixedHeight = height()*BUTTON_HEIGHT_RATIO;
        button->setIconSize(QSize(2*fixedWidth/3, 2*fixedHeight/3));

        switch (button->mode().key) {
        case Qt::Key_Backspace:
            button->setIcon(keyIcon(KeyIcon::Backspace));
            break;
        case Qt::Key_CapsLock:
            button->setIcon(keyIcon(KeyIcon::CapsLock));
            connect(button, SIGNAL(pressed()), this, SLOT(switchCapsLock()), Qt::UniqueConnection);
            break;
        case Qt::Key_Mode_switch:
            connect(button, SIGNAL(pressed()), this, SLOT(switchSpecialChar()), Qt::UniqueConnection);
            break;
        case Qt::Key_Context2:
            // 布局切换键不连发：长按会来回切布局
            button->setAutoRepeat(false);
            fitKeyFont(button, 26, false, kLayoutKeyStretch);
            break;
        case Qt::Key_Context1:
            // 切换键长按连发会反复翻转中英态（键帽「汉/en」状态标识跟着抖），关掉连发；
            // 字母/退格等输入键的连发是刻意保留的
            button->setAutoRepeat(false);
            fitKeyFont(button, 30, true, kLangKeyStretch);
            connect(button, SIGNAL(pressed()), this, SLOT(switchEnOrCh()), Qt::UniqueConnection);
            break;
        case Qt::Key_Enter:
            button->setIcon(keyIcon(KeyIcon::Enter));
            break;
        case Qt::Key_Space:
            button->setIcon(keyIcon(KeyIcon::Space));
            break;
        default:
            break;
        }
    }
}


ChineseWidget::ChineseWidget(QWidget *parent) :
    QListWidget(parent)
{
    // 字典随模板 qrc 内置（见 resources/TouchKeyboard.qrc），无条件加载：
    // 中文输入法是固能力，不做宏开关（旧版三个 ENABLED_*_CHINESE_LIB 宏无任何
    // 定义方，迁移 PUPSIT 时曾因删除宿主 define 导致三表全被编译出去、候选
    // 全空——宏开关正是这类静默失效的根因，故整体移除）
    loadChineseLib();
    loadChinesePhraseLib();
    loadGoogleChineseLib();

    // 空库总检：三个字典全部没 load 上来时候选栏只剩原始拼音（中文输入静默
    // 失效）。qWarning 不受 QT_NO_DEBUG_OUTPUT 影响，发布构建也能在日志里看见
    if (m_data.isEmpty())
        qWarning() << "拼音字典全部为空：中文候选输入不可用（检查 qrc 资源路径/alias）";

    setFocusPolicy(Qt::NoFocus);
    /* 设置为列表显示模式 */
    setViewMode(QListView::ListMode);

    /* 从左往右排列 */
    setFlow(QListView::LeftToRight);

    /* 屏蔽水平滑动条 */
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    /* 屏蔽垂直滑动条 */
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    /* 设置为像素滚动 */
    setHorizontalScrollMode(QListWidget::ScrollPerPixel);
    /* 竖向也必须 ScrollPerPixel：默认 ScrollPerItem 的量程是「0..(条数−可见条数)」格，
     * QScroller 按像素算出的甩动距离压进这几格里会被归一化成"要么不动、要么直接到头"
     * （aerocount 坑 39：用户实测「滚动很奇怪，一下子全都翻过去了」）。 */
    setVerticalScrollMode(QListWidget::ScrollPerPixel);
    /* 手势用 TouchGesture 而非 LeftMouseButtonGesture：后者连鼠标按住都当甩动，
     * 桌面宿主上选候选要按很久才能点中 */
    QScroller::grabGesture(this, QScroller::TouchGesture);
    /* 甩动参数：起拖延迟/阈值压到触摸可用，帧率拉满（候选区一屏三五条，
     * 掉帧会显得"粘手"） */
    QScrollerProperties prop = QScroller::scroller(this)->scrollerProperties();
    prop.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0);
    prop.setScrollMetric(QScrollerProperties::DragStartDistance, 0.001);
    prop.setScrollMetric(QScrollerProperties::FrameRate, QScrollerProperties::FrameRates::Fps60);
    QScroller::scroller(this)->setScrollerProperties(prop);
    /* 拖动期间不误触条目：按下即进拖动态时，抬起不再当 click */
    viewport()->installEventFilter(this);

    /* 配色走 AeaKeyboardStyle 常量表 + palette（禁 QSS）：条目 hover/selected 的
     * 高亮底与原 QSS #4395ff 同值，文字色同族；绘制由 CandidateItemDelegate 自绘
     * （见本文件下方），不再靠 QListWidget::Item 选择器 */
    QPalette pal = palette();
    pal.setColor(QPalette::Text, AeaQt::candidateInkColor());
    pal.setColor(QPalette::Highlight, AeaQt::candidateHighlightColor());
    pal.setColor(QPalette::HighlightedText, AeaQt::candidateHighlightInkColor());
    pal.setColor(QPalette::Base, AeaQt::candidateIdleFaceColor());
    setPalette(pal);
    setAutoFillBackground(false);
    setItemDelegate(new CandidateItemDelegate(this));

    connect(this, SIGNAL(itemClicked(QListWidgetItem *)), this, SLOT(onItemClicked(QListWidgetItem *)));
}

/// 拖动期间吞掉 mouseRelease：QScroller 已识别为拖动时，抬起不当作条目点击
/// （否则一甩动就点中一个候选，拼音被提交掉）。
bool ChineseWidget::eventFilter(QObject *watched, QEvent *e)
{
    if (watched == viewport() && e->type() == QEvent::MouseButtonRelease) {
        QScroller *sc = QScroller::scroller(this);
        if (sc && sc->state() == QScroller::Dragging)
            return true;
    }
    return QListWidget::eventFilter(watched, e);
}

/// 候选条目自绘（禁 QSS 后 QListWidget::Item:hover/selected 的替代）：
/// 圆角高亮底 + 同族文字色，未选中透明底（与原 QSS #00000000 一致）。
void CandidateItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    QPainter *p = painter;
    p->setRenderHint(QPainter::Antialiasing, true);
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    if (selected || hovered) {
        p->save();
        p->setPen(Qt::NoPen);
        p->setBrush(selected ? AeaQt::candidateHighlightColor()
                            : AeaQt::candidateHighlightColor().lighter(115));
        p->drawRoundedRect(option.rect.adjusted(1, 1, -1, -1), 4, 4);
        p->restore();
    }
    QStyleOptionViewItem opt = option;
    opt.palette.setColor(QPalette::Text,
                         selected ? AeaQt::candidateHighlightInkColor()
                                 : AeaQt::candidateInkColor());
    QStyledItemDelegate::paint(p, opt, index);
}

void ChineseWidget::setText(const QString &text)
{
    /* clear() 即删除全部旧候选项（原 takeItem(i)+delete 循环每次取走后索引
     * 前移会隔项漏删，且删除本就由 clear() 统一完成——整段可直接去掉） */
    clear();

    /* 空缓冲直接返回：onItemClicked 提交候选后调 setText("") 只为清空，
     * 原写法会补一条空文本条目占在候选行里（提交后候选区留一条空白） */
    if (text.isEmpty())
        return;

    addOneItem(text);

    if (! m_data.contains(text.left(1))) {
        return;
    }

    /* 通过获取首字母索引词库内容，用于加快匹配词(组)。 */
    const QList<QPair<QString, QString> > &tmp = m_data[text.left(1)];
    for (int i = 0; i < tmp.count(); i++) {
        const QPair<QString, QString> &each = tmp.at(i);
        /* 模糊匹配 */
        if (each.first.left(text.count()) != text)
            continue;

        /* 添加到候选栏, 并限制数量 */
        if (this->count() <= 30) {
            addOneItem(each.second);
        }
        else {
            break;
        }
    }
}

void ChineseWidget::onItemClicked(QListWidgetItem *item)
{
    emit pressedChanged(-1, item->text());
    setText("");
}

void ChineseWidget::addOneItem(const QString &text)
{
    QListWidgetItem *item = new QListWidgetItem(text, this);
    // 候选条目像素字号：宿主可按字号系数下发（setCapPixels），未下发用模板默认值。
    // 原 QSS 时代的 setPointSize(18) 是点字号（DPI 相关），禁 QSS 后统一改像素；
    // 条目宽也改成按 QFontMetrics 实测（原用 pointSize 当宽度代理，与实际渲染不同源）。
    const int capPx = capPixels() > 0 ? capPixels() : kCandidateFontPixels;
    QFont font;
    font.setPixelSize(capPx);
    font.setWeight(QFont::Normal);   // setBold(true) 会被本行覆盖成 Normal，矛盾调用已删
    item->setFont(font);

    /* 设置文字居中 */
    item->setTextAlignment(Qt::AlignCenter);
    const bool isChinese = QRegularExpression(QStringLiteral("^[\u4E00-\u9FA5]+"))
            .match(text.left(1)).hasMatch();

    const QFontMetrics fm(font);
    const int textWidth = fm.horizontalAdvance(text);
    const int width = qMax(kCandidateItemMinWidth,
                           textWidth + 2 * kCandidateItemPadding);

    /* 条目高度与 QSS 的 Item height 一致（36px，落进 40px 候选区视口） */
    item->setSizeHint(QSize(width, kCandidateItemHeight));
    addItem(item);
}

void ChineseWidget::setCapPixels(int px)
{
    if (m_capPixels == px)
        return;
    m_capPixels = px;
    // 已入列的候选条目同步换字号：语言/字号系数切换时不重建列表，
    // 只改新条目会让同一行候选一半大一半小
    const int capPx = px > 0 ? px : kCandidateFontPixels;
    QFont font = this->font();
    font.setPixelSize(capPx);
    for (int i = 0; i < count(); ++i) {
        QListWidgetItem *entry = QListWidget::item(i);
        if (entry)
            entry->setFont(font);
    }
}

void ChineseWidget::loadChineseLib()
{
    QFile pinyin;
    if (! openTouchKbdDict(pinyin, QStringLiteral(TOUCHKBD_PINYIN_DICT))) {
        qWarning() << "拼音单字字典打开失败（中文候选不可用）：" << TOUCHKBD_PINYIN_DICT;
        return;
    }

    while (! pinyin.atEnd()) {
        QString buf = QString::fromUtf8(pinyin.readLine()).trimmed();
        const QRegularExpression regExp(QStringLiteral("^[\u4E00-\u9FA5]+"));
        if (!regExp.match(buf).hasMatch())
            continue;

        const QRegularExpressionMatch singleMatch = regExp.match(buf);
        QString first = buf.right(buf.size() - singleMatch.capturedLength());
        QString second = singleMatch.captured(0);

        QList<QPair<QString, QString> > &tmp = m_data[first.left(1)];
        tmp.append(qMakePair(first, second));
    }

    pinyin.close();
}

void ChineseWidget::loadChinesePhraseLib()
{
    /* 加载词组字库内容 */
    QFile pinyin;
    if (! openTouchKbdDict(pinyin, QStringLiteral(TOUCHKBD_PINYIN_PHRASE_DICT))) {
        qWarning() << "拼音词组字典打开失败（词组候选不可用）：" << TOUCHKBD_PINYIN_PHRASE_DICT;
        return;
    }

    /* 按行读取内容 */
    while (! pinyin.atEnd()) {
        QString buf = QString::fromUtf8(pinyin.readLine()).trimmed();
        if (buf.isEmpty())
            continue;

        /* 去除#号后的注释内容 */
        if (buf.left(1) == "#")
            continue;

        /* 大词库版（41 万条）：逐行正则 globalMatch 开销不可接受（实测启动多花
         * 数秒），改字符串切分——格式固定「词组: 拼音」，indexOf(':') 一刀两断即
         * 可，语义与正则版一致（词组侧无冒号） */
        const int colon = buf.indexOf(QLatin1Char(':'));
        if (colon <= 0)
            continue;
        const QString second = buf.left(colon);           /* 词组 */
        QString first = buf.mid(colon + 1).trimmed();     /* 拼音（remove 去空格需非 const） */

        if (second.isEmpty() || first.isEmpty())
            continue;

        QStringList strList = first.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        QString abb;
        abb.reserve(strList.size());
        for (const QString &syllable : strList) {
            /* 获得拼音词组的首字母(用于缩写匹配) */
            if (!syllable.isEmpty())
                abb.append(syllable.at(0));
        }
        if (abb.isEmpty())
            continue;
        QList<QPair<QString, QString> > &tmp = m_data[first.left(1)];
        /* 将'拼音(缩写)'和'词组'写入匹配容器 */
        tmp.append(qMakePair(abb, second));
        /* 将'拼音(全拼)'和'词组'写入匹配容器 */
        tmp.append(qMakePair(QString(first).remove(QLatin1Char(' ')), second));
    }

    pinyin.close();
}

void ChineseWidget::loadGoogleChineseLib()
{
    QFile file;
    if (! openTouchKbdDict(file, QStringLiteral(TOUCHKBD_GOOGLE_DICT))) {
        qWarning() << "谷歌字典打开失败（大字表候选不可用）：" << TOUCHKBD_GOOGLE_DICT;
        return;
    }

    QTextStream in(&file);
#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
    in.setEncoding(QStringConverter::Encoding::Utf16); // UTF-16（BOM 自适应）
#else
    in.setCodec("UTF-16"); // Qt5 等价 API（BOM 自适应；setEncoding 为 Qt6 起才有）
#endif

    QStringList lines = in.readAll().split("\n");
    if (lines.constLast().isEmpty())
        lines.removeLast();

    /* 逐行解析「汉字 权重 未知 拼音」——正则提到循环外只编译一次：
     * 6.5 万行每行现构造 QRegularExpression 会把模式重编译一遍，启动明显变慢 */
    static const QRegularExpression re(QStringLiteral(R"RX((\S+).((?:-?\d+)(?:\.\d+)).((?:-?\d+)(?:\.\d+)?).(.*))RX"));
    for (const QString &each : lines) {
        bool isMatching = false;
        auto dictIt = re.globalMatch(each);
        while (dictIt.hasNext()) {
            const QRegularExpressionMatch dictMatch = dictIt.next();
            isMatching = true;
            QString hanzi = dictMatch.captured(1); // 汉字
            QString weight = dictMatch.captured(2); // 权重
            QString tmp = dictMatch.captured(3); // 未知
            QString pinyin = dictMatch.captured(4); // 拼音(可能是词组)

            QStringList strList = pinyin.split(" ");
            QString abb;
            abb.reserve(strList.count());
            for (int i = 0; i < strList.count(); i++) {
                /* 获得拼音词组的首字母(用于缩写匹配) */
                abb += strList.at(i).left(1);
            }

            QList<QPair<QString, QString> > &list = m_data[pinyin.left(1)];
            if (strList.count() > 1) {
                /* 将'拼音(缩写)'和'词组'写入匹配容器 */
                list.append(qMakePair(abb, hanzi));
            }
            /* 将'拼音(全拼)'和'词组'写入匹配容器 */
            list.append(qMakePair(pinyin.remove(" "), hanzi));
        }

        if (!isMatching)
            qDebug() << each;
    }

    file.close();
}
