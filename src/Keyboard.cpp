#include "Keyboard.h"
#include "KeyButton.h"
#include <QVBoxLayout>
#include <QFile>
#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
#include <QStringConverter>
#endif
#include <QApplication>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
#include <QScroller>
#endif

#include <QRegularExpression>
#include <QDebug>
#include "ui_Keyboard.h"


using namespace AeaQt;

typedef QList<KeyButton::Mode> Modes;
typedef QList<Modes> ModesList;

const int NORMAL_BUTTON_WIDTH  = 45;
const int NORMAL_BUTTON_HEIGHT = 45;

const QString BACKSPACE_ICON = ":/touchkeyboard/icons/backspace.png";
const QString ENTER_ICON     = ":/touchkeyboard/icons/enter.png";
const QString SPACE_ICON     = ":/touchkeyboard/icons/space.png";
const QString CAPLOCK_ICON   = ":/touchkeyboard/icons/caplock.png";

// 中文输入三字典（qrc 路径，随模板 qrc 编入——中文输入是固有能力，非可选项）。
// 资源路径与模板 resources/TouchKeyboard.qrc 的三个 qresource 前缀一一对应
// （qrc 侧必须写 alias 映射成扁平路径，详见 openDictFile 上方注释）。
// 要换自定义字典：.pro 里 -D 覆盖宏，或直接改这里。
#ifndef TOUCHKBD_PINYIN_DICT
#define TOUCHKBD_PINYIN_DICT       ":/ChineseLib/pinyin.txt"
#endif
#ifndef TOUCHKBD_PINYIN_PHRASE_DICT
#define TOUCHKBD_PINYIN_PHRASE_DICT ":/ChinesePhraseLib/pinyin_phrase.txt"
#endif
#ifndef TOUCHKBD_GOOGLE_DICT
#define TOUCHKBD_GOOGLE_DICT       ":/GoogleChineseLib/rawdict_utf16_65105_freq_sort.txt"
#endif

// 字典资源打开：扁平路径（qrc 写了 alias 的形态，模板 qrc 现状）优先，
// 打不开再回落「prefix/dicts/文件名」的嵌套路径试一次。
// 缘由：qrc 漏写 alias 时真实路径会多出 /dicts 一节，三个字典 open 全部失败，
// 而失败提示原是 qDebug（发布构建 QT_NO_DEBUG_OUTPUT 下被整段编掉）——静默
// 失效表现为「候选栏只剩原始拼音、中文输入全废」。PUPSIT(2026-09-29)与
// PressureMonitor_noflow(2026-10-09)两次现场反馈同一根因。此处双路径兜底：
// 宿主 qrc 即使漏 alias 也能加载，不把正确性全押在资源配置上。
static bool openDictFile(QFile &file, const QString &flatPath)
{
    file.setFileName(flatPath);
    if (file.open(QIODevice::ReadOnly))
        return true;

    const int slash = flatPath.lastIndexOf(QLatin1Char('/'));
    if (slash <= 0)
        return false;
    file.setFileName(flatPath.left(slash) + QStringLiteral("/dicts") + flatPath.mid(slash));
    return file.open(QIODevice::ReadOnly);
}

const double BUTTON_SPACING_RATIO = 0.030;
const double BUTTON_WIDTH_RATIO   = 0.09;
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
    {{Qt::Key_Backspace, "", ""/*退格*/}}
};

const QList<Modes> modeListBar4 = {
    {{Qt::Key_Mode_switch, "",  "?123"}},
    // 中/英切换键：display 与标题栏状态标识（m_langStatus）同一口径「en/汉」，
    // 两处是同一个 m_isChinese 的镜像，避免用户看成两个不同功能
    {{Qt::Key_Context1,    "",  "en"},    {Qt::Key_Context1, "", QStringLiteral("汉")}},
    {{Qt::Key_Space,       " ", ""/*空格*/}},
    {{Qt::Key_Enter,       "",  ""/*换行*/}},
    // 清空折进底行（与 PUPSIT 全键盘 Clear+Enter 同形态；确认不单列——回车即确认，
    // 二者行为完全相同，单列属冗余。数字小键盘无回车键，其底行才保留清空/确认）
    {{Qt::Key_Clear,       "", QString::fromUtf8("清空")}},
};

Keyboard::Keyboard(QWidget *parent) :
    AbstractKeyboard(parent),
    ui(new Ui::Keyboard),
    m_isChinese(false)
{
    ui->setupUi(this);

    // 面板换肤与 logo 文本按品牌常量套用（.ui 里写死的青底/WOWA 仅作 Designer 预览默认值，
    // 宿主改 AbstractKeyboard.h 的 brandPanelColor()/brandLogoText() 一处即全模板生效）
    setStyleSheet(QStringLiteral(
        "QWidget#keyBoard { background-color: %1; border-radius: 15px; border: 1px solid gray; }"
        "#frame { background-color: %1; border-radius: 15px; border: 1px solid gray; }")
        .arg(brandPanelColor().name()));
    if (ui->labPY)
        ui->labPY->setText(brandLogoText());

    // 标题栏中/英状态标识（大字白字，点击切换）：紧跟品牌文本、候选区左侧。
    // 放候选区内侧会在英文态被 candidateContainer 一起收起（setVisible(false)）——
    // 那正是用户最需要看见"当前是英文"的时候，故固定常驻标题栏。与底行中/英
    // 键帽同源：两处都是 m_isChinese 的镜像，点击任一处都走 switchEnOrCh。
    // 白字与 labPY/candidateContainer 同款走 QSS color（本模板白-on-青文本的
    // 统一做法；祖先带 QSS 时 palette 路线表现不稳，不混用）；flat 不画边框/底色
    m_langStatus = new QPushButton(this);
    m_langStatus->setFlat(true);
    m_langStatus->setFocusPolicy(Qt::NoFocus);
    m_langStatus->setCursor(Qt::PointingHandCursor);
    m_langStatus->setFont(QFont(QStringLiteral("Microsoft YaHei"), 20, QFont::Bold));
    m_langStatus->setMinimumSize(60, 36);
    m_langStatus->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    m_langStatus->setStyleSheet(QStringLiteral("color: rgb(255, 255, 255);"));
    if (ui->horizontalLayout)
        ui->horizontalLayout->insertWidget(1, m_langStatus);
    connect(m_langStatus, &QPushButton::clicked, this, &Keyboard::switchEnOrCh);
    updateLangIndicator();

    // 真圆角窗口：QSS 圆角只裁 #frame 内容，窗口四角仍露底层显成方角——
    // 透明背景 + 圆角 mask 从窗口层裁掉（半径与 #frame 的 border-radius 一致）
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
    QHBoxLayout *row3Layout = qobject_cast<QHBoxLayout*>(ui->row3Container->layout());
    if (row3Layout) {
        for (int i = 0; i < modeListBar3.count(); ++i) {
            KeyButton *button = createButton(modeListBar3.at(i));
            if (i == 0 || i == modeListBar3.count() - 1) {
                // 第一个和最后一个按钮拉伸因子 70
                row3Layout->addWidget(button, 70);
            } else {
                row3Layout->addWidget(button, 69);
            }
        }
    }

    // 第四行（不同按钮不同拉伸因子）
    QHBoxLayout *row4Layout = qobject_cast<QHBoxLayout*>(ui->row4Container->layout());
    if (row4Layout) {
        for (int i = 0; i < modeListBar4.count(); ++i) {
            KeyButton *button = createButton(modeListBar4.at(i));
            int stretch = 0;
            if (i == 0) stretch = 12;
            else if (i == 1) stretch = 10;
            else if (i == 2) stretch = 56;
            else stretch = 22;   // 回车与清空等宽
            row4Layout->addWidget(button, stretch);
        }
    }

    // 标题栏按钮设置（保持原有）
    QPushButton *closeBtn = ui->btnClose;
    if (closeBtn) {
        closeBtn->setFocusPolicy(Qt::NoFocus);
        connect(closeBtn, &QPushButton::clicked, this, &Keyboard::hide);
    }

    // 可选：设置标题栏其他按钮焦点策略
    QList<QWidget*> titleWidgets = ui->titleBar->findChildren<QWidget*>();
    foreach (QWidget *w, titleWidgets) {
        w->setFocusPolicy(Qt::NoFocus);
    }

    // 调用 resizeButton 设置图标和特殊连接
    resizeButton();
}

// ========== 新增析构函数，释放 UI 对象 ==========
Keyboard::~Keyboard()
{
    delete ui;
}
// =============================================

void Keyboard::resizeEvent(QResizeEvent *e)
{
    resizeButton();
    // 圆角 mask 随尺寸重算；判重防 setMask 触发二次 resize 时反复重入
    const QRegion m = roundedWindowMask(size(), kCornerRadius);
    if (mask() != m)
        setMask(m);
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
    foreach(KeyButton *button, buttons)
        button->switchSpecialChar();
}

void Keyboard::switchEnOrCh()
{
    m_isChinese = !m_isChinese;

    if (! m_isChinese) {
        // 离开中文态：丢弃未提交的拼音缓冲与候选残留——否则切回中文时候选栏
        // 还挂着上一串拼音的结果， buffer 也会接着旧串往下拼
        clearBufferText();
        m_chineseWidget->clear();
    }

    updateLangIndicator();

    // 底行中/英键帽文本跟随（en/汉 两态循环，每切一次翻一态，与 m_isChinese 同步；
    // 初始态与 m_modes.first()「en」一致，故构造期只调 updateLangIndicator 不循环）
    for (KeyButton *button : findChildren<KeyButton *>()) {
        if (button->mode().key == Qt::Key_Context1)
            button->switching();
    }
}

void Keyboard::updateLangIndicator()
{
    // 候选区仅中文态常驻：英文态收起（容器高度随之归 0，不占标题栏空间）
    ui->candidateContainer->setVisible(m_isChinese);

    // 标题栏状态标识：中文「汉」/英文「en」——20pt 加粗白字，明显大于键帽文字，
    // 颜色与标题栏品牌文本一致（白）。文案说清"现在是什么态 + 点一下去哪里"
    if (m_langStatus) {
        m_langStatus->setText(m_isChinese ? QStringLiteral("汉") : QStringLiteral("en"));
        m_langStatus->setToolTip(m_isChinese
            ? QStringLiteral("中文输入：字母键拼拼音出候选，点击切换为英文")
            : QStringLiteral("英文输入：字母键直接上屏，点击切换为中文"));
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
    // 清空：清拼音缓冲 + 清空焦点输入框（与数字小键盘清空键同语义）
    if (code == Qt::Key_Clear) {
        clearBufferText();
        if (auto *edit = qobject_cast<QLineEdit*>(QApplication::focusWidget()))
            edit->clear();
        else if (auto *te = qobject_cast<QTextEdit*>(QApplication::focusWidget()))
            te->clear();
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

void Keyboard::resizeButton()
{
    foreach (KeyButton *button, findChildren<KeyButton *>()) {
        int fixedWidth = width()*BUTTON_WIDTH_RATIO;
        int fixedHeight = height()*BUTTON_HEIGHT_RATIO;
        button->setIconSize(QSize(2*fixedWidth/3, 2*fixedHeight/3));

        switch (button->mode().key) {
        case Qt::Key_Backspace:
            button->setIcon(QIcon(BACKSPACE_ICON));
            break;
        case Qt::Key_CapsLock:
            button->setIcon(QIcon(CAPLOCK_ICON));
            connect(button, SIGNAL(pressed()), this, SLOT(switchCapsLock()), Qt::UniqueConnection);
            break;
        case Qt::Key_Mode_switch:
            connect(button, SIGNAL(pressed()), this, SLOT(switchSpecialChar()), Qt::UniqueConnection);
            break;
        case Qt::Key_Context1:
            // 切换键长按连发会反复翻转中英态（标题栏状态标识跟着抖），关掉连发；
            // 字母/退格等输入键的连发是刻意保留的
            button->setAutoRepeat(false);
            connect(button, SIGNAL(pressed()), this, SLOT(switchEnOrCh()), Qt::UniqueConnection);
            break;
        case Qt::Key_Enter:
            button->setIcon(QIcon(ENTER_ICON));
            break;
        case Qt::Key_Space:
            button->setIcon(QIcon(SPACE_ICON));
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

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    /* 设置鼠标左键拖动 */
    QScroller::grabGesture(this, QScroller::LeftMouseButtonGesture);
#endif

    /* 设置样式。条目高度 36px：候选区（标题栏内 candidateContainer）最高 40px，
     * 原 50px 条目会被底边裁掉一截（悬停/选中蓝底缺底边、文字贴顶）——36px
     * 整条落进 40px 视口，18pt 文字仍有充裕留白 */
    setStyleSheet("                                                                           \
                  QListWidget { outline: none; border:1px solid #00000000; color: black; }    \
                  QListWidget::Item { width: 50px; height: 36px; }                            \
                  QListWidget::Item:hover { background: #4395ff; color: white; }              \
                  QListWidget::item:selected { background: #4395ff; color: black; }           \
                  QListWidget::item:selected:!active { background: #00000000; color: black; } \
                  ");

    connect(this, SIGNAL(itemClicked(QListWidgetItem *)), this, SLOT(onItemClicked(QListWidgetItem *)));
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
    QFont font;
    font.setPointSize(18);
    font.setWeight(QFont::Normal);   // setBold(true) 会被本行覆盖成 Normal，矛盾调用已删
    item->setFont(font);

    /* 设置文字居中 */
    item->setTextAlignment(Qt::AlignCenter);
    const bool isChinese = QRegularExpression(QStringLiteral("^[\u4E00-\u9FA5]+"))
            .match(text.left(1)).hasMatch();

    int width = font.pointSize();
    if (isChinese)
        width += text.count()*font.pointSize()*1.5;
    else
        width += text.count()*font.pointSize()*2/3;

    /* 条目高度与 QSS 的 Item height 一致（36px，落进 40px 候选区视口） */
    item->setSizeHint(QSize(width, 36));
    addItem(item);
}

void ChineseWidget::loadChineseLib()
{
    QFile pinyin;
    if (! openDictFile(pinyin, QStringLiteral(TOUCHKBD_PINYIN_DICT))) {
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
    if (! openDictFile(pinyin, QStringLiteral(TOUCHKBD_PINYIN_PHRASE_DICT))) {
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
    if (! openDictFile(file, QStringLiteral(TOUCHKBD_GOOGLE_DICT))) {
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
