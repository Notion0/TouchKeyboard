#include "Keyboard.h"
#include "KeyButton.h"
#include <QVBoxLayout>
#include <QFile>
#include <QStringConverter>
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

// 中文输入三字典（qrc 路径）。模板不内置 8MB 级字典资产，由宿主工程提供同路径资源；
// 缺失时三个 load* 均优雅降级（打 qDebug 后返回，键盘其余功能不受影响），
// 不需要中文输入的宿主可就此不管。要改路径在工程 .pro 里 -D 覆盖，或直接改这里。
#ifndef TOUCHKBD_PINYIN_DICT
#define TOUCHKBD_PINYIN_DICT       ":/ChineseLib/pinyin.txt"
#endif
#ifndef TOUCHKBD_PINYIN_PHRASE_DICT
#define TOUCHKBD_PINYIN_PHRASE_DICT ":/ChinesePhraseLib/pinyin_phrase.txt"
#endif
#ifndef TOUCHKBD_GOOGLE_DICT
#define TOUCHKBD_GOOGLE_DICT       ":/GoogleChineseLib/rawdict_utf16_65105_freq_sort.txt"
#endif

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
    {{Qt::Key_Context1,    "",  "En"},    {Qt::Key_Context1, "", QStringLiteral("中")}},
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

    // 真圆角窗口：QSS 圆角只裁 #frame 内容，窗口四角仍露底层白底显成方角——
    // 透明背景 + 圆角 mask 从窗口层裁掉（半径与 #frame 的 border-radius 一致）
    setMask(roundedWindowMask(size(), 15));

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
            this, &Keyboard::onKeyPressed);
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
    const QRegion m = roundedWindowMask(size(), 15);
    if (mask() != m)
        setMask(m);
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
    ui->candidateContainer->setVisible(m_isChinese);   // 显示或隐藏容器

    QList<KeyButton *> buttons = findChildren<KeyButton *>();
    foreach(KeyButton *button, buttons) {
        if (button->mode().key == Qt::Key_Context1) {
            button->switching();
        }
    }
}

void Keyboard::onButtonPressed(const int &code, const QString &text)
{

    if (code == Qt::Key_Enter || code == Qt::Key_Return) {
        QWidget *w = QApplication::focusWidget();
        if (w) {
            // 如果是 QLineEdit，触发完成信号
            if (auto lineEdit = qobject_cast<QLineEdit *>(w)) {
                emit lineEdit->editingFinished();
            }
            // 关键：让输入框失去焦点（否则仍处于编辑状态）
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
        m_bufferText.append(text);
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

QWidget *Keyboard::createBar(const QList<QList<KeyButton::Mode> > &modes)
{
    QWidget *widget = new QWidget;

    QHBoxLayout *h = new QHBoxLayout;
    for (int i = 0; i < modes.count(); i++) {
        KeyButton *button = createButton(modes.at(i));
        h->addWidget(button);
    }

    widget->setLayout(h);
    return widget;
}

QWidget *Keyboard::chineseBar()
{
    m_chineseWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    return m_chineseWidget;
}


QWidget *Keyboard::candidateList()
{
    return m_chineseWidget;
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
#ifdef ENABLED_CHINESE_LIB
    loadChineseLib();
#endif

#ifdef ENABLED_CHINESE_PHRASE_LIB
    loadChinesePhraseLib();
#endif

#ifdef ENABLED_GOOGLE_CHINESE_LIB
    loadGoogleChineseLib();
#endif

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

    /* 设置样式 */
    setStyleSheet("                                                                           \
                  QListWidget { outline: none; border:1px solid #00000000; color: black; }    \
                  QListWidget::Item { width: 50px; height: 50px; }                            \
                  QListWidget::Item:hover { background: #4395ff; color: white; }              \
                  QListWidget::item:selected { background: #4395ff; color: black; }           \
                  QListWidget::item:selected:!active { background: #00000000; color: black; } \
                  ");

    connect(this, SIGNAL(itemClicked(QListWidgetItem *)), this, SLOT(onItemClicked(QListWidgetItem *)));
}

void ChineseWidget::setText(const QString &text)
{
    for (int i = 0; i < count(); i++) {
        QListWidgetItem *item = takeItem(i);
        delete item;
        item = NULL;
    }

    clear();

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
    font.setBold(true);
    font.setWeight(QFont::Normal);
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

    item->setSizeHint(QSize(width, 50));
    addItem(item);
}

void ChineseWidget::loadChineseLib()
{
    QFile pinyin(TOUCHKBD_PINYIN_DICT);
    if (! pinyin.open(QIODevice::ReadOnly)) {
        qDebug() << "Open pinyin file failed!";
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
    QFile pinyin(TOUCHKBD_PINYIN_PHRASE_DICT);
    if (! pinyin.open(QIODevice::ReadOnly)) {
        qDebug() << "Open pinyin file failed!";
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

        /* 正则匹配词组内容并通过组捕获获取'词组'和'拼音' */
        const QRegularExpression regExp(QStringLiteral("(\\S+): ([\\S ]+)"));
        auto phraseIt = regExp.globalMatch(buf);
        while (phraseIt.hasNext()) {
            const QRegularExpressionMatch phraseMatch = phraseIt.next();
            QString second = phraseMatch.captured(1);  /* 词组 */
            QString first = phraseMatch.captured(2); /* 拼音 */

            QStringList strList = first.split(" ");
            QString abb;
            for (int i = 0; i < strList.count(); i++) {
                /* 获得拼音词组的首字母(用于缩写匹配) */
                abb += strList.at(i).left(1);
            }
            QList<QPair<QString, QString> > &tmp = m_data[first.left(1)];
            /* 将'拼音(缩写)'和'词组'写入匹配容器 */
            tmp.append(qMakePair(abb, second));
            /* 将'拼音(全拼)'和'词组'写入匹配容器 */
            tmp.append(qMakePair(first.remove(" "), second));
        }
    }

    pinyin.close();
}

void ChineseWidget::loadGoogleChineseLib()
{
    QFile file(TOUCHKBD_GOOGLE_DICT);
    if (! file.open(QIODevice::ReadOnly)) {
        qDebug() << "Open pinyin file failed!" << file.fileName();
        return;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Encoding::Utf16); // UTF-16（BOM 自适应）

    QStringList lines = in.readAll().split("\n");

    for (QString each : lines) {
        const QRegularExpression re(QStringLiteral(R"RX((\S+).((?:-?\d+)(?:\.\d+)).((?:-?\d+)(?:\.\d+)?).(.*))RX"));
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
