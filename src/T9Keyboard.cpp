#include "T9Keyboard.h"

#include <QFontMetrics>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>

#include <algorithm>

using namespace AeaQt;

namespace {

// 键帽配色（模板常量；面板底色取 brandPanelColor()，与另两个键盘同款）
const QColor kKeyFace(255, 255, 255);
const QColor kKeyFunction(222, 238, 237);
const QColor kKeyPressed(190, 205, 204);
const QColor kKeyActive(20, 110, 106);
const QColor kKeyText(51, 51, 51);
const QColor kKeySubText(120, 120, 120);

const int kKeyboardWidth = 672;
const int kKeyboardHeight = 360;
const int kCompHeight = 48;
const int kCandHeight = 56;
const int kGridTop = kCompHeight + kCandHeight;   // 104
const int kRowHeight = 64;
const int kMargin = 4;
const int kSideWidth = 92;
const int kMidWidth = 160;
const int kGap = 3;           // 键帽之间的留白（绘制内缩，不改几何）
const int kDragThreshold = 10;

const int kMaxInputDigits = 32;  // 数字序列上限（与中文拼音缓冲同口径）
const int kMaxCandidates = 12;   // 候选栏最多词条（其余可滚动）
const int kMinLearnedLength = 3; // 短于该长度的直选序列不学习
const int kMaxLearnedWords = 200;  // 学过的词缓存上限（输入法缓存，非业务数据）

// 内置常用词：工业 HMI 常用英文 + 高频普通词。按空格分隔，启动时split。
// 业务词由宿主 setExtraWords 注入，用户输过的词进 QSettings，均优先于本表。
const QString kBuiltinWordText = QStringLiteral(
    "admin administrator user users password login logout root guest "
    "operator supervisor manager engineer technician auditor "
    "sample sampler sampling room rooms point points area zone location "
    "plan recipe batch lot filter flow pressure valve pump motor sensor "
    "chamber clean verify calibration calibrate status alarm error "
    "warning fault event record records data log logs audit report "
    "export import print save cancel delete edit add new open close "
    "start stop pause resume run setting settings config system device "
    "comm communication port serial uart protocol version firmware "
    "time date year month day hour minute second timer count total "
    "volume duration delay interval mode auto manual enable disable "
    "true false yes name code number type value unit level rate speed "
    "test testing check pass fail quality control standard method "
    "result results note notes comment reason signature approve review "
    "confirm apply reset default factory maintenance service support "
    "about info "
    "the and for are but not you all any can had her was one our out "
    "day get has him his how man now old see two way who boy did its "
    "let put say she too use bad big box car cat cow cup dog eat egg "
    "end far few fit fix fun gun hat hit hot job key kid law lay leg "
    "lie lot low map may mix near need next nice once only onto over "
    "page pay per pick play pull push rain read real rest ride ring "
    "rise road rock role roll rule safe said sail salt same sand sat "
    "save seat seem seen self sell send sent ship shoe shop show shut "
    "sick side sign sing sit site size skin sleep slow snow soft soil "
    "sold sole song soon sort sound south space speak speed spell "
    "spend spot stand star stay step stick still stone stood store "
    "storm story study stuff style suit summer sure sweet table take "
    "talk tall teach team tell ten term text than that them then they "
    "thin thing think third this those three threw throw tie tiny tip "
    "tire title today told tone tonight took tool top total touch "
    "toward town track trade train treat tree trip truck true try "
    "turn under unit until upon used usual very visit voice wait wake "
    "walk wall want warm wash watch water wave wear weather week "
    "weight west wheel where whether which while white whole whose "
    "why wide wife wild will win wind window wine wing winter wish "
    "with within without woman women wonder wood word work world "
    "would write wrong yard year yellow yet young your");

} // namespace

T9Key::T9Key(QWidget *parent)
    : QAbstractButton(parent)
{
    setFocusPolicy(Qt::NoFocus);
}

void T9Key::setLabels(const QString &main, const QString &sub)
{
    setText(main);
    m_sub = sub;
    update();
}

void T9Key::setRole(Role role)
{
    m_role = role;
    update();
}

void T9Key::setActive(bool on)
{
    if (m_active == on)
        return;
    m_active = on;
    update();
}

void T9Key::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = QRectF(rect()).adjusted(kGap, kGap, -kGap, -kGap);
    QColor face = m_role == Function ? kKeyFunction : kKeyFace;
    QColor ink = kKeyText;
    if (m_active) {
        face = kKeyActive;
        ink = Qt::white;
    }
    if (isDown())
        face = kKeyPressed;
    if (!isEnabled())
        ink = kKeySubText;

    QPainterPath path;
    path.addRoundedRect(r, 6, 6);
    p.fillPath(path, face);

    QFont mainFont = font();
    mainFont.setPixelSize(text().size() > 2 ? 18 : 24);
    p.setFont(mainFont);
    p.setPen(ink);
    if (m_sub.isEmpty()) {
        p.drawText(r, Qt::AlignCenter, text());
        return;
    }

    // 主字偏上、副字在下（数字 + 字母组）
    const QRectF top(r.left(), r.top() + 4, r.width(), r.height() * 0.55);
    const QRectF bottom(r.left(), r.top() + r.height() * 0.55, r.width(), r.height() * 0.40);
    p.drawText(top, Qt::AlignHCenter | Qt::AlignBottom, text());
    QFont subFont = font();
    subFont.setPixelSize(14);
    p.setFont(subFont);
    p.setPen(m_active ? QColor(Qt::white) : kKeySubText);
    p.drawText(bottom, Qt::AlignHCenter | Qt::AlignTop, m_sub);
}

T9Strip::T9Strip(Qt::Orientation orientation, QWidget *parent)
    : QWidget(parent)
    , m_orientation(orientation)
    , m_background(Qt::transparent)
    , m_text(kKeyText)
    , m_accent(kKeyActive)
{
    setFocusPolicy(Qt::NoFocus);
}

void T9Strip::setItems(const QStringList &items)
{
    m_items = items;
    m_highlight = -1;
    m_scroll = 0;
    relayoutItems();
    update();
}

void T9Strip::setPlaceholder(const QString &text)
{
    m_placeholder = text;
    update();
}

void T9Strip::setHighlight(int index)
{
    m_highlight = index;
    update();
}

void T9Strip::setFontPixelSize(int px)
{
    m_fontPx = px;
    relayoutItems();
    update();
}

void T9Strip::setColors(const QColor &background, const QColor &text, const QColor &accent)
{
    m_background = background;
    m_text = text;
    m_accent = accent;
    update();
}

void T9Strip::resizeEvent(QResizeEvent *)
{
    relayoutItems();
}

int T9Strip::axis(const QPoint &p) const
{
    return m_orientation == Qt::Horizontal ? p.x() : p.y();
}

int T9Strip::viewLength() const
{
    return m_orientation == Qt::Horizontal ? width() : height();
}

int T9Strip::maxScroll() const
{
    if (m_starts.isEmpty())
        return 0;
    const int content = m_starts.last() + m_lengths.last();
    return qMax(0, content - viewLength());
}

void T9Strip::relayoutItems()
{
    m_starts.clear();
    m_lengths.clear();
    QFont f = font();
    f.setPixelSize(m_fontPx);
    const QFontMetrics fm(f);
    int pos = 0;
    for (const QString &item : m_items) {
        // 横向按文字宽 + 左右留白（单字也不小于 48，保证可点）；纵向固定行高
        const int len = m_orientation == Qt::Horizontal
                ? qMax(48, fm.horizontalAdvance(item) + 28)
                : 44;
        m_starts.append(pos);
        m_lengths.append(len);
        pos += len;
    }
    m_scroll = qBound(0, m_scroll, maxScroll());
}

int T9Strip::itemAt(const QPoint &p) const
{
    const int a = axis(p) + m_scroll;
    for (int i = 0; i < m_starts.size(); ++i) {
        if (a >= m_starts.at(i) && a < m_starts.at(i) + m_lengths.at(i))
            return i;
    }
    return -1;
}

void T9Strip::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (m_background.alpha() > 0) {
        QPainterPath bg;
        bg.addRoundedRect(QRectF(rect()).adjusted(kGap, kGap, -kGap, -kGap), 6, 6);
        p.fillPath(bg, m_background);
    }

    QFont f = font();
    f.setPixelSize(m_fontPx);
    p.setFont(f);

    if (m_items.isEmpty()) {
        if (!m_placeholder.isEmpty()) {
            QColor c = m_text;
            c.setAlpha(140);
            p.setPen(c);
            p.drawText(rect().adjusted(12, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft,
                       m_placeholder);
        }
        return;
    }

    for (int i = 0; i < m_items.size(); ++i) {
        const int start = m_starts.at(i) - m_scroll;
        const int len = m_lengths.at(i);
        if (start + len < 0 || start > viewLength())
            continue;
        const QRect cell = m_orientation == Qt::Horizontal
                ? QRect(start, 0, len, height())
                : QRect(0, start, width(), len);
        if (i == m_highlight) {
            QPainterPath hl;
            hl.addRoundedRect(QRectF(cell).adjusted(4, 6, -4, -6), 6, 6);
            p.fillPath(hl, m_accent);
            p.setPen(Qt::white);
        } else {
            p.setPen(m_text);
        }
        p.drawText(cell, Qt::AlignCenter, m_items.at(i));
    }
}

void T9Strip::mousePressEvent(QMouseEvent *event)
{
    m_pressed = true;
    m_dragging = false;
    m_pressAxis = axis(event->pos());
    m_pressScroll = m_scroll;
}

void T9Strip::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_pressed)
        return;
    const int delta = axis(event->pos()) - m_pressAxis;
    if (!m_dragging && qAbs(delta) < kDragThreshold)
        return;
    m_dragging = true;
    m_scroll = qBound(0, m_pressScroll - delta, maxScroll());
    update();
}

void T9Strip::mouseReleaseEvent(QMouseEvent *event)
{
    const bool click = m_pressed && !m_dragging;
    m_pressed = false;
    m_dragging = false;
    if (!click)
        return;
    const int index = itemAt(event->pos());
    if (index >= 0)
        emit itemClicked(index);
}

T9Keyboard::T9Keyboard(QWidget *parent)
    : AbstractKeyboard(parent)
{
    setFixedSize(kKeyboardWidth, kKeyboardHeight);

    loadLearned();   // 学过的词（QSettings）并入词表，优先于内置表

    // K3：词库后台加载；加载完成前候选栏显示"词库加载中…"，组合栏照常可用
    m_engine = new T9PinyinEngine(this);
    connect(m_engine, &T9PinyinEngine::ready, this, [this] {
        if (!m_input.isEmpty())
            refreshChinese();
    });
    m_engine->loadAsync();

    m_compBar = new T9Strip(Qt::Horizontal, this);
    m_compBar->setFontPixelSize(18);
    m_compBar->setColors(Qt::transparent, Qt::white, kKeyActive);
    connect(m_compBar, &T9Strip::itemClicked, this, &T9Keyboard::onCompositionClicked);

    m_candBar = new T9Strip(Qt::Horizontal, this);
    m_candBar->setFontPixelSize(22);
    m_candBar->setColors(kKeyFace, kKeyText, kKeyActive);
    connect(m_candBar, &T9Strip::itemClicked, this, &T9Keyboard::onCandidateClicked);

    m_punctStrip = new T9Strip(Qt::Vertical, this);
    m_punctStrip->setFontPixelSize(22);
    m_punctStrip->setColors(kKeyFunction, kKeyText, kKeyActive);
    connect(m_punctStrip, &T9Strip::itemClicked, this, &T9Keyboard::onPunctClicked);

    for (int id = 0; id < KeyCount; ++id)
        m_keys.append(addKey(id));
    m_keys.at(KeyBack)->setAutoRepeat(true);
    m_keys.at(KeyBack)->setAutoRepeatDelay(500);
    m_keys.at(KeyBack)->setAutoRepeatInterval(80);

    for (int i = 0; i < 24; ++i) {
        T9Key *key = new T9Key(this);
        connect(key, &QAbstractButton::clicked, this, [this, i] { onSymbolClicked(i); });
        m_symbolKeys.append(key);
    }

    // 键盘随输入框切换常驻可见（不经 hide），换框时学掉上一个框输成词、清掉缓冲
    connect(qApp, &QApplication::focusChanged, this, [this] {
        learnPendingWord();
        resetComposition();
        updateEcho();
        // 宿主场景词：输入框可挂 "t9Words" 动态属性（QStringList），聚焦即生效；
        // 没挂该属性的字段保持当前词集不动（如登录页喂的账户 ID）
        if (QWidget *w = QApplication::focusWidget()) {
            const QVariant v = w->property("t9Words");
            if (v.isValid())
                setExtraWords(v.toStringList());
        }
    });

    layoutKeys();
    applyMode();
}

T9Keyboard::~T9Keyboard() = default;

T9Key *T9Keyboard::addKey(int id)
{
    T9Key *key = new T9Key(this);
    const bool function = id == KeySymbol || id == KeyNum || id == KeyLang
            || id == KeyBack || id == KeyClear || id == KeyEnter || id == KeyLayout;
    key->setRole(function ? T9Key::Function : T9Key::Normal);
    connect(key, &QAbstractButton::clicked, this, [this, id] { onKey(id); });
    return key;
}

void T9Keyboard::layoutKeys()
{
    const int left = kMargin;
    const int mid = kMargin + kSideWidth;
    const int right = mid + 3 * kMidWidth;
    auto rowY = [](int row) { return kGridTop + row * kRowHeight; };

    m_compBar->setGeometry(kMargin, 0, width() - 2 * kMargin, kCompHeight);
    m_candBar->setGeometry(kMargin, kCompHeight, width() - 2 * kMargin, kCandHeight);

    m_keys.at(KeySymbol)->setGeometry(left, rowY(0), kSideWidth, kRowHeight);
    m_punctStrip->setGeometry(left + kGap, rowY(1) + kGap,
                              kSideWidth - 2 * kGap, 2 * kRowHeight - 2 * kGap);
    m_keys.at(KeyNum)->setGeometry(left, rowY(3), kSideWidth, kRowHeight);

    for (int n = 1; n <= 9; ++n) {
        const int row = (n - 1) / 3;
        const int col = (n - 1) % 3;
        m_keys.at(KeyD1 + n - 1)->setGeometry(mid + col * kMidWidth, rowY(row),
                                              kMidWidth, kRowHeight);
    }
    // 末行三键各占一列：中/英 | 0 空格 | 全键盘（布局切换，回 26 键）。
    // aerocount 分叉线删掉大小写键后是两键各 1.5 列；合回模板并存时第三列给布局切换
    m_keys.at(KeyLang)->setGeometry(mid, rowY(3), kMidWidth, kRowHeight);
    m_keys.at(KeyD0)->setGeometry(mid + kMidWidth, rowY(3), kMidWidth, kRowHeight);
    m_keys.at(KeyLayout)->setGeometry(mid + 2 * kMidWidth, rowY(3), kMidWidth, kRowHeight);

    m_keys.at(KeyBack)->setGeometry(right, rowY(0), kSideWidth, kRowHeight);
    m_keys.at(KeyClear)->setGeometry(right, rowY(1), kSideWidth, kRowHeight);
    m_keys.at(KeyEnter)->setGeometry(right, rowY(2), kSideWidth, 2 * kRowHeight);

    // 符号页：左列 + 中间三列、上三行，8 列 x 3 行
    const int symbolWidth = (right - left) / 8;
    for (int i = 0; i < m_symbolKeys.size(); ++i) {
        m_symbolKeys.at(i)->setGeometry(left + (i % 8) * symbolWidth, rowY(i / 8),
                                        symbolWidth, kRowHeight);
    }
}

void T9Keyboard::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()), kCornerRadius, kCornerRadius);
    p.fillPath(path, brandPanelColor());

    if (m_echoActive) {
        // 回显画在组合栏区域（该栏已让位隐藏）；超长靠左省略，保留最新输入
        const QRect r(kMargin + 12, kGap, width() - 2 * kMargin - 24, kCompHeight - 2 * kGap);
        QFont f = font();
        f.setPixelSize(20);
        p.setFont(f);
        p.setPen(kKeyText);
        p.drawText(r, Qt::AlignVCenter | Qt::AlignLeft,
                   QFontMetrics(f).elidedText(m_echoText, Qt::ElideLeft, r.width()));
    }
}

void T9Keyboard::resizeEvent(QResizeEvent *)
{
    const QRegion m = roundedWindowMask(size(), kCornerRadius);
    if (mask() != m)
        setMask(m);
}

void T9Keyboard::hideEvent(QHideEvent *event)
{
    learnPendingWord();
    resetComposition();
    AbstractKeyboard::hideEvent(event);
}

QStringList T9Keyboard::punctuation() const
{
    // 数字页同样把 @ 放首位（2026-10-26 现场反馈：数字界面找不到 @，
    // 输带 @ 的密码要来回切页）
    if (m_mode == Digits)
        return {QStringLiteral("@"), QStringLiteral("+"), QStringLiteral("-"), QStringLiteral("*"),
                QStringLiteral("/"), QStringLiteral("%"), QStringLiteral("="),
                QStringLiteral(":"), QStringLiteral("("), QStringLiteral(")")};
    // @ 提到首位（2026-10-26 现场反馈「@ 藏得太深」）：左侧竖条只露约 2.8 项，
    // 原先 @ 排第 8 要滚到底才看得到；账户/邮箱/密码里 @ 是最常用的英文符号
    if (m_mode == English)
        return {QStringLiteral("@"), QStringLiteral(","), QStringLiteral("."),
                QStringLiteral("?"), QStringLiteral("!"), QStringLiteral(":"),
                QStringLiteral(";"), QStringLiteral("'")};
    return {QStringLiteral("，"), QStringLiteral("。"), QStringLiteral("？"),
            QStringLiteral("！"), QStringLiteral("、"), QStringLiteral("："),
            QStringLiteral("；"), QStringLiteral("…")};
}

QStringList T9Keyboard::symbolPage() const
{
    if (m_symbolPage == 0) {
        return QString::fromUtf8("，。？！、：；…“”‘’（）《》—·￥【】～「」")
                .split(QString(), Qt::SkipEmptyParts);
    }
    // @ 放英文符号页首位（同上，2026-10-26）
    return QString::fromLatin1("@,.?!:;'\"()[]{}<>#$%&*_~")
            .split(QString(), Qt::SkipEmptyParts);
}

void T9Keyboard::setMode(Mode mode)
{
    learnPendingWord();
    resetComposition();
    if (mode == Chinese || mode == English)
        m_textMode = mode;
    m_mode = mode;
    applyMode();
}

void T9Keyboard::applyMode()
{
    static const char *const groups[10] = {
        "", "", "abc", "def", "ghi", "jkl", "mno", "pqrs", "tuv", "wxyz"
    };
    const bool text = m_mode == Chinese || m_mode == English;
    const bool symbols = m_mode == Symbols;

    for (int n = 1; n <= 9; ++n) {
        T9Key *key = m_keys.at(KeyD1 + n - 1);
        key->setVisible(!symbols);
        QString sub;
        if (m_mode == Chinese)
            sub = n == 1 ? tr("分词") : QString::fromLatin1(groups[n]).toUpper();
        else if (m_mode == English)
            sub = n == 1 ? QStringLiteral("@.,?!") : QString::fromLatin1(groups[n]);
        key->setLabels(QString::number(n), sub);
    }

    m_keys.at(KeySymbol)->setVisible(!symbols);
    m_keys.at(KeySymbol)->setLabels(tr("符号"));
    m_punctStrip->setVisible(!symbols);
    m_punctStrip->setItems(punctuation());
    // 数字/符号页里该键是"回文本模式"：键帽直接写回到哪（拼音/英文），
    // 不写笼统的「返回」——操作员一眼知道按下去是什么输入法（2026-10-26 现场反馈）
    m_keys.at(KeyNum)->setLabels(text ? QStringLiteral("123")
                                      : (m_textMode == Chinese ? tr("拼音") : tr("英文")));

    T9Key *lang = m_keys.at(KeyLang);
    T9Key *zero = m_keys.at(KeyD0);
    if (m_mode == Digits) {
        lang->setLabels(QStringLiteral("."));
        zero->setLabels(QStringLiteral("0"));
    } else if (symbols) {
        lang->setLabels(m_symbolPage == 0 ? tr("英文符号") : tr("中文符号"));
        zero->setLabels(tr("空格"));
    } else {
        lang->setLabels(m_mode == Chinese ? tr("中") : QStringLiteral("En"), tr("中/英"));
        zero->setLabels(QStringLiteral("0"), tr("空格"));
    }

    const QStringList page = symbolPage();
    for (int i = 0; i < m_symbolKeys.size(); ++i) {
        T9Key *key = m_symbolKeys.at(i);
        key->setVisible(symbols && i < page.size());
        if (i < page.size())
            key->setLabels(page.at(i));
    }

    // 中/英键只在文本模式下受英文-only 约束（数字页它是「.」、符号页是翻页，照常可用）
    lang->setEnabled(!(m_englishOnly && text));
    m_keys.at(KeyLayout)->setLabels(tr("全键盘"));
    m_keys.at(KeyLayout)->setVisible(!symbols);

    m_keys.at(KeyBack)->setLabels(tr("退格"));
    m_keys.at(KeyClear)->setLabels(tr("清空"));
    m_keys.at(KeyEnter)->setLabels(tr("确认"));

    if (m_mode == Chinese) {
        // 组合栏可见性交给各分支：中文按回显状态、英文在 refreshEnglish 里定
        m_compBar->setVisible(!m_echoActive);
        refreshChinese();
    } else if (m_mode == English) {
        refreshEnglish();
    } else {
        m_compBar->setVisible(false);
        m_compBar->setItems({});
        m_candBar->setItems({});
        m_candBar->setPlaceholder(QString());
    }
}

namespace {

// 英文数字键的字符序列（候选栏字母直选与中文模式副标签共用）
QString tapLetters(int digit)
{
    static const char *const table[10] = {
        "", "@.,?!1", "abc2", "def3", "ghi4", "jkl5", "mno6", "pqrs7", "tuv8", "wxyz9"
    };
    return digit >= 1 && digit <= 9 ? QString::fromLatin1(table[digit]) : QString();
}

} // namespace

void T9Keyboard::commitText(const QString &text)
{
    // 记录上屏目标是不是密码框：learnPendingWord 在焦点切换时也会调，那时焦点
    // 已是新字段，不能当场判断，只能在提交这一刻记下
    const QLineEdit *target = qobject_cast<const QLineEdit *>(QApplication::focusWidget());
    m_lastTargetPassword = target && target->echoMode() == QLineEdit::Password;
    onKeyPressed(text == QLatin1String(" ") ? int(Qt::Key_Space) : int(Qt::Key_unknown), text);
    updateEcho();
}

void T9Keyboard::commitEnglishItem(const QString &item, bool isWord)
{
    if (isWord)
        m_typedWord.clear();   // 整词上屏，字母缓冲作废（词已在词表里）
    else
        m_typedWord.append(item);
    commitText(item);
    m_engInput.clear();
    refreshEnglish();
}

void T9Keyboard::sendBackspace()
{
    // 退到输入框内容了：直选字母缓冲作废，避免把删改中的片段学成错词
    m_typedWord.clear();
    onKeyPressed(Qt::Key_Backspace, QString());
    updateEcho();
}

void T9Keyboard::onKey(int id)
{
    switch (id) {
    case KeyEnter: {
        // 确认 = 主动失焦 + 收起（与旧全键盘同一路径；失焦时 QLineEdit 自发一次
        // editingFinished，不要再手动 emit，否则双发——T11）
        learnPendingWord();
        resetComposition();
        if (QWidget *w = QApplication::focusWidget())
            w->clearFocus();
        hide();
        return;
    }
    case KeyClear: {
        resetComposition();
        QWidget *w = QApplication::focusWidget();
        if (auto *edit = qobject_cast<QLineEdit *>(w))
            edit->clear();
        else if (w && w->inherits("QTextEdit"))
            QMetaObject::invokeMethod(w, "clear");
        updateEcho();
        return;
    }
    case KeyBack:
        // 退格：中文缓冲 / 英文数字序列非空先删缓冲；否则删输入框里的字
        if (m_mode == Chinese && !m_input.isEmpty()) {
            m_input.chop(1);
            m_locked.clear();
            refreshChinese();
            return;
        }
        if (m_mode == English && !m_engInput.isEmpty()) {
            m_engInput.chop(1);
            refreshEnglish();
            return;
        }
        sendBackspace();
        return;
    case KeySymbol:
        m_symbolPage = m_textMode == English ? 1 : 0;
        setMode(Symbols);
        return;
    case KeyNum:
        setMode(m_mode == Chinese || m_mode == English ? Digits : m_textMode);
        return;
    case KeyLang:
        if (m_mode == Digits) {
            commitText(QStringLiteral("."));
        } else if (m_mode == Symbols) {
            m_symbolPage ^= 1;
            applyMode();
        } else if (!m_englishOnly) {
            setMode(m_mode == Chinese ? English : Chinese);
        }
        return;
    case KeyLayout:
        // 只发请求：InputManager 收起九宫格、弹 26 键（排队执行，不在按键回调里自隐藏）
        learnPendingWord();
        resetComposition();
        emit layoutSwitchRequested();
        return;
    case KeyD0:
        if (m_mode == Digits) {
            commitText(QStringLiteral("0"));
        } else if (m_mode == Chinese) {
            // 有拼音时 0 上屏首选（手机九宫格习惯），无拼音时是空格
            if (!commitFirstCandidate())
                commitText(QStringLiteral(" "));
        } else {
            learnPendingWord();
            resetComposition();
            commitText(QStringLiteral(" "));
        }
        return;
    default:
        break;
    }

    const int digit = id - KeyD1 + 1;
    if (digit < 1 || digit > 9)
        return;
    if (m_mode == Digits)
        commitText(QString::number(digit));
    else if (m_mode == Chinese)
        onDigit(digit);
    else if (m_mode == English)
        onEnglishDigit(digit);
}

void T9Keyboard::onEnglishDigit(int digit)
{
    if (m_engInput.size() >= kMaxInputDigits)
        return;
    m_engInput.append(char('0' + digit));
    refreshEnglish();
}

void T9Keyboard::refreshEnglish()
{
    if (m_mode != English)
        return;
    // 候选栏：数字序列匹配的词（学习 → 业务 → 内置，见 matchWords）。
    // 密码框不联想单词（2026-10-26 aerocount 板上反馈）：①字母组原先排在 12 个词之后，
    // 候选栏一屏只看得见 admin/administrator… 这类词，按 2 看不到「a b c 2 A B C」；
    // ②密码框给出常见词联想等于提示弱口令。密码框候选栏只放末位键的字母直选
    const QStringList words = m_echoActive ? QStringList() : matchWords(m_engInput);
    m_engWordCount = words.size();

    // 顶部组合栏：末位数字键的字母直选——小写在前、大写在后（a b c A B C），
    // 点选即上屏，没有多击的时间限制。密码场景该区域让给回显，字母并入候选栏。
    QStringList letters;
    if (!m_engInput.isEmpty()) {
        // 顺序 a b c A B C 2：小写一组、大写一组、最后数字/标点（原实现交错成
        // a A b B c C，与本注释及现场期望「abcABC」不符，2026-10-26 改正）
        const QString group = tapLetters(m_engInput.right(1).toInt());
        QStringList upper;
        QStringList others;
        for (const QChar c : group) {
            if (c.isLetter()) {
                letters.append(c);
                upper.append(c.toUpper());
            } else {
                others.append(c);
            }
        }
        letters << upper << others;
    }

    m_compBar->setVisible(!m_echoActive);
    if (m_echoActive) {
        m_compBar->setItems({});
        m_candBar->setItems(words + letters);
    } else {
        m_compBar->setItems(letters);
        m_candBar->setItems(words);
    }
    m_compBar->setHighlight(-1);
    m_candBar->setHighlight(-1);
    m_candBar->setPlaceholder(QString());
    update();   // 回显文字画在父窗口上，子栏重绘带不到它
}

void T9Keyboard::setEnglishOnly(bool enabled)
{
    if (m_englishOnly == enabled)
        return;
    m_englishOnly = enabled;
    if (enabled && m_textMode == Chinese) {
        // 先退出中文再上锁；在数字/符号页时只改返回目标，不打断当前页
        if (m_mode == Chinese) {
            setMode(English);
            return;   // setMode 内已 applyMode
        }
        m_textMode = English;
    }
    applyMode();
}

bool T9Keyboard::isChineseMode() const
{
    return m_textMode == Chinese;
}

void T9Keyboard::setChineseMode(bool chinese)
{
    if (chinese && m_englishOnly)
        return;
    const Mode target = chinese ? Chinese : English;
    if (m_mode == Chinese || m_mode == English) {
        if (m_mode != target)
            setMode(target);
    } else {
        m_textMode = target;   // 数字/符号页：返回时落到目标文本模式
    }
}

void T9Keyboard::setExtraWords(const QStringList &words)
{
    m_extra = words;
    rebuildWords();
    if (m_mode == English)
        refreshEnglish();
}

void T9Keyboard::updateEcho()
{
    // 密码框回显：字段里是圆点，键盘上显示真实已输内容，输错了当场看得见
    const QLineEdit *edit = qobject_cast<const QLineEdit *>(QApplication::focusWidget());
    const bool active = edit && edit->echoMode() == QLineEdit::Password;
    const QString text = active ? edit->text() : QString();
    if (active == m_echoActive && text == m_echoText)
        return;
    m_echoActive = active;
    m_echoText = text;
    if (m_mode == English) {
        refreshEnglish();   // 字母在「组合栏 / 候选栏尾部」之间换位
    } else {
        m_compBar->setVisible(m_mode == Chinese && !m_echoActive);
        update();
    }
}

QByteArray T9Keyboard::encodeWord(const QString &word) const
{
    QByteArray out;
    out.reserve(word.size());
    for (const QChar c : word.toLower()) {
        if (c >= QLatin1Char('a') && c <= QLatin1Char('c'))
            out.append('2');
        else if (c >= QLatin1Char('d') && c <= QLatin1Char('f'))
            out.append('3');
        else if (c >= QLatin1Char('g') && c <= QLatin1Char('i'))
            out.append('4');
        else if (c >= QLatin1Char('j') && c <= QLatin1Char('l'))
            out.append('5');
        else if (c >= QLatin1Char('m') && c <= QLatin1Char('o'))
            out.append('6');
        else if (c >= QLatin1Char('p') && c <= QLatin1Char('s'))
            out.append('7');
        else if (c >= QLatin1Char('t') && c <= QLatin1Char('v'))
            out.append('8');
        else if (c >= QLatin1Char('w') && c <= QLatin1Char('z'))
            out.append('9');
        else if (c >= QLatin1Char('0') && c <= QLatin1Char('9'))
            out.append(c.toLatin1());   // 代码里的数字原样进编码（Sch01 → 72401，
                                        // 否则按满串永远匹配不上带数字的代码）
        // 其余非字母数字（空格/连字符/中文）不进编码
    }
    return out;
}

void T9Keyboard::rebuildWords()
{
    m_words.clear();
    QStringList seen;   // 小写去重：学习 → 业务 → 内置，先到先得
    const QStringList builtin =
            kBuiltinWordText.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const auto appendUnique = [&](const QString &word) {
        const QString key = word.toLower();
        if (word.isEmpty() || seen.contains(key))
            return;
        seen.append(key);
        m_words.append(qMakePair(word, encodeWord(word)));
    };
    for (const QString &w : m_learned)
        appendUnique(w);
    for (const QString &w : m_extra)
        appendUnique(w);
    for (const QString &w : builtin)
        appendUnique(w);
}

QStringList T9Keyboard::matchWords(const QByteArray &seq) const
{
    if (seq.isEmpty())
        return {};
    QStringList exact;
    QStringList prefix;
    for (const auto &w : m_words) {
        if (w.second == seq)
            exact.append(w.first);
        else if (w.second.startsWith(seq))
            prefix.append(w.first);
    }
    // 前缀匹配短词优先（更接近输入目标），组内保持来源优先级
    std::stable_sort(prefix.begin(), prefix.end(), [](const QString &a, const QString &b) {
        return a.size() < b.size();
    });
    QStringList result = exact + prefix;
    while (result.size() > kMaxCandidates)
        result.removeLast();
    return result;
}

void T9Keyboard::learnWord(const QString &word)
{
    if (word.size() < kMinLearnedLength)
        return;
    const QString key = word.toLower();
    for (const auto &w : m_words) {
        if (w.first.toLower() == key)
            return;   // 词表里已有（内置/业务/学过的），不重复记
    }
    m_learned.prepend(word);
    // 学过的词是输入法缓存不是业务数据，设上限防无限增长（超出丢最旧）
    while (m_learned.size() > kMaxLearnedWords)
        m_learned.removeLast();
    rebuildWords();
    saveLearned();
}

void T9Keyboard::learnPendingWord()
{
    if (m_typedWord.size() < kMinLearnedLength) {
        m_typedWord.clear();
        return;
    }
    // 密码框输入的内容不进词表：明文落配置有暴露风险，密码也不该进候选
    if (!m_lastTargetPassword)
        learnWord(m_typedWord);
    m_typedWord.clear();
}

void T9Keyboard::loadLearned()
{
    const QSettings settings;
    m_learned = settings.value(QStringLiteral("T9Keyboard/learnedWords")).toStringList();
    rebuildWords();
}

void T9Keyboard::saveLearned() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("T9Keyboard/learnedWords"), m_learned);
}

void T9Keyboard::onDigit(int digit)
{
    if (digit == 1) {
        // 1 = 分词符：只在两段数字之间有意义
        if (m_input.isEmpty() || m_input.endsWith('\''))
            return;
        m_input.append('\'');
    } else {
        if (T9PinyinEngine::stripSeparators(m_input).size() >= 32)
            return;
        m_input.append(char('0' + digit));
    }
    m_locked.clear();
    refreshChinese();
}

void T9Keyboard::resetComposition()
{
    m_input.clear();
    m_engInput.clear();
    m_typedWord.clear();
    m_locked.clear();
    m_comps.clear();
    m_cands.clear();
    if (m_mode == Chinese) {
        m_compBar->setItems({});
        m_candBar->setItems({});
        m_candBar->setPlaceholder(QString());
    } else if (m_mode == English) {
        refreshEnglish();
    }
}

void T9Keyboard::refreshChinese()
{
    if (m_mode != Chinese)
        return;

    m_comps = m_engine->compositions(m_input);
    m_cands.clear();
    if (m_engine->isReady() && !m_input.isEmpty())
        m_cands = m_engine->candidates(m_input, m_locked, 60);

    m_compBar->setItems(m_comps);
    int lockedIndex = -1;
    for (int i = 0; i < m_comps.size() && !m_locked.isEmpty(); ++i) {
        if (QString(m_comps.at(i)).remove(QLatin1Char('\'')) == m_locked) {
            lockedIndex = i;
            break;
        }
    }
    m_compBar->setHighlight(lockedIndex);

    QStringList words;
    words.reserve(m_cands.size());
    for (const T9Candidate &c : m_cands)
        words.append(c.word);
    m_candBar->setItems(words);
    m_candBar->setPlaceholder(!m_input.isEmpty() && !m_engine->isReady()
                                  ? tr("词库加载中…") : QString());
}

void T9Keyboard::onCompositionClicked(int index)
{
    if (m_mode == English) {
        // 英文：组合栏是字母直选，点一个上屏一个，数字序列作废
        const QStringList items = m_compBar->items();
        if (index < 0 || index >= items.size())
            return;
        commitEnglishItem(items.at(index), false);
        return;
    }
    if (m_mode != Chinese || index < 0 || index >= m_comps.size())
        return;
    // 点组合栏 = 锁定拼音过滤候选；再点一次同一项解除
    const QString py = QString(m_comps.at(index)).remove(QLatin1Char('\''));
    m_locked = m_locked == py ? QString() : py;
    refreshChinese();
}

void T9Keyboard::onCandidateClicked(int index)
{
    if (index < 0)
        return;
    if (m_mode == Chinese) {
        if (index >= m_cands.size())
            return;
        const T9Candidate chosen = m_cands.at(index);
        commitText(chosen.word);
        // 扣掉该词占用的数字，剩余数字继续出候选（可连续选词）
        m_input = T9PinyinEngine::consume(m_input, chosen.digitCount);
        m_locked.clear();
        refreshChinese();
        return;
    }
    if (m_mode != English)
        return;
    const QStringList items = m_candBar->items();
    if (index >= items.size())
        return;
    // 密码场景字母并入候选栏：前 m_engWordCount 项是词，其余是字母
    commitEnglishItem(items.at(index), index < m_engWordCount);
}

bool T9Keyboard::commitFirstCandidate()
{
    if (m_input.isEmpty())
        return false;
    if (!m_cands.isEmpty())
        onCandidateClicked(0);
    return true;
}

void T9Keyboard::onPunctClicked(int index)
{
    const QStringList list = punctuation();
    if (index < 0 || index >= list.size())
        return;
    if (m_mode == Chinese) {
        commitFirstCandidate();
        resetComposition();
    } else {
        learnPendingWord();
        m_engInput.clear();
        refreshEnglish();
    }
    commitText(list.at(index));
}

void T9Keyboard::onSymbolClicked(int index)
{
    const QStringList page = symbolPage();
    if (index >= 0 && index < page.size()) {
        learnPendingWord();
        commitText(page.at(index));
    }
}
