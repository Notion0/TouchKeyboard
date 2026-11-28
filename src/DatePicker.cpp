// Keyboard/DatePicker.cpp
// 见 DatePicker.h。零 QSS：面板 / 标题栏 / 圆角 mask / 全部格位一律 QPainter 自绘，
// 颜色只取 AeaKeyboardStyle.h 与 AbstractKeyboard.h 的 inline 取色函数。
#include "DatePicker.h"

#include "AeaKeyboardStyle.h"
#include "AbstractKeyboard.h"

#include <QApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace AeaQt {

// ---------------------------------------------------------------------------
// 规格（固定尺寸，勿改：三宿主共用同一面板；改一处要三处同步视觉预期）
// ---------------------------------------------------------------------------
constexpr int kPanelWidth = 460;
constexpr int kPanelHeight = 444;
constexpr int kTitleHeight = 48;    ///< 顶部标题栏（含 44px 收起钮）
constexpr int kNavHeight = 44;      ///< 年月导航栏
constexpr int kBottomHeight = 44;   ///< 底栏（今天 / 取消）
constexpr int kStackHeight = kPanelHeight - kTitleHeight - kNavHeight - kBottomHeight;  // 308
constexpr int kWeekdayHeight = 28;  ///< 星期行（28 + 6×46 + 下边距 4 = 堆栈高 308）
constexpr int kDayCellHeight = (kStackHeight - kWeekdayHeight) / 6;   ///< 46 ≥44 触控红线
constexpr int kPanelRadius = 12;    ///< 面板圆角（同心圆角阶梯：卡片档）
constexpr int kCellRadius = 8;      ///< 格位圆角（按钮档）
constexpr int kGridGap = 6;         ///< 年/月九宫格间距
constexpr int kCellInset = 3;       ///< 格位自绘内缩（视觉留白，不占命中区）

constexpr int kTitlePointSize = 16;    ///< 标题栏日期
constexpr int kNavPointSize = 14;      ///< 导航与底栏按钮
constexpr int kCellPointSize = 14;     ///< 日期格
constexpr int kBigCellPointSize = 15;  ///< 年/月大格
constexpr int kWeekPointSize = 12;     ///< 星期行

// ===========================================================================
// PickerButton
// ===========================================================================
PickerButton::PickerButton(Role role, QWidget* parent)
    : QPushButton(parent)
    , m_role(role)
{
    // 不抢焦点：点击后焦点若离开宿主输入框，InputManager::focusChanged 会把面板收起
    setFocusPolicy(Qt::NoFocus);
    // 禁自动填充背景：Qt 会在自绘圆角之前铺一层矩形底色，把四角填方
    setAutoFillBackground(false);
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(role == Role::Hide ? kNavHeight : kDayCellHeight);
}

void PickerButton::setEmphasis(bool on)
{
    if (m_emphasis == on)
        return;
    m_emphasis = on;
    update();
}

void PickerButton::setToday(bool on)
{
    if (m_today == on)
        return;
    m_today = on;
    update();
}

void PickerButton::setMuted(bool on)
{
    if (m_muted == on)
        return;
    m_muted = on;
    update();
}

void PickerButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    // 不调 QPushButton::paintEvent：Fusion 会先画一层按钮面板（底 + 边框），
    // 与这里的自绘格位叠成双重框。KeyButton 同口径（全自绘、不走基类）
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const bool enabled = isEnabled();
    const bool down = isDown();
    const bool hover = underMouse() && enabled && !down;

    // 标题栏收起钮：直接摆键盘同款矢量图标（标题栏白字色），不画任何文字
    if (m_role == Role::Hide) {
        const int iconS = 24;
        keyIcon(KeyIcon::Hide, titleInkColor()).paint(
            &p, QRect(rect().center().x() - iconS / 2, rect().center().y() - iconS / 2,
                      iconS, iconS));
        return;
    }

    // 导航钮的矢量箭头（« ‹ › »）：既画矢量也不画字符——字符在缺字形平台上是豆腐块，
    // 双画则显成「箭头套箭头」
    const QString t = text();
    const bool isArrow = (t == QStringLiteral("«") || t == QStringLiteral("‹")
                          || t == QStringLiteral("›") || t == QStringLiteral("»"));
    if (isArrow) {
        if (down || hover) {
            p.setPen(Qt::NoPen);
            p.setBrush(down ? keyCapPressedMutedColor() : keyCapFunctionFaceColor());
            p.drawRoundedRect(rect().adjusted(3, 3, -3, -3), kCellRadius, kCellRadius);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(enabled ? keyCapInkColor() : keyCapDisabledInkColor());
        const bool right = (t == QStringLiteral("›") || t == QStringLiteral("»"));
        const bool dbl = (t == QStringLiteral("«") || t == QStringLiteral("»"));
        const qreal cx = rect().center().x();
        const qreal cy = rect().center().y();
        const qreal s = 5.0;
        for (int i = 0; i < (dbl ? 2 : 1); ++i) {
            const qreal off = dbl ? (i == 0 ? -4.0 : 4.0) : 0.0;
            const qreal x = cx + (right ? -off : off);
            QPainterPath tri;
            if (right) {
                tri.moveTo(x - s, cy - s);
                tri.lineTo(x + s, cy);
                tri.lineTo(x - s, cy + s);
            } else {
                tri.moveTo(x + s, cy - s);
                tri.lineTo(x - s, cy);
                tri.lineTo(x + s, cy + s);
            }
            tri.closeSubpath();
            p.drawPath(tri);
        }
        return;
    }

    // ---- 取色（全部来自模板取色函数，禁散落 hex）----
    QColor ink = keyCapInkColor();
    if (!enabled)
        ink = keyCapDisabledInkColor();
    else if (m_emphasis)
        ink = titleInkColor();           // 实心块上反白
    else if (m_muted)
        ink = keyCapSubInkColor();
    else if (m_today)
        ink = brandPanelColor();

    // ---- 底：实心主题色块 / 今日圆环 / 按压悬停浅底 ----
    const QRectF box = rect().adjusted(kCellInset, kCellInset, -kCellInset, -kCellInset);
    if (m_role == Role::Plain) {
        // 导航与底栏：常态透明（不抢视线），按压/悬停给一层浅青
        if (down || hover) {
            p.setPen(Qt::NoPen);
            p.setBrush(down ? keyCapPressedMutedColor() : keyCapFunctionFaceColor());
            p.drawRoundedRect(box, kCellRadius, kCellRadius);
        }
    } else if (m_emphasis && enabled) {
        p.setPen(Qt::NoPen);
        p.setBrush(brandPanelColor());
        p.drawRoundedRect(box, kCellRadius, kCellRadius);
    } else if (m_today && enabled) {
        p.setPen(QPen(brandPanelColor(), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(box, kCellRadius, kCellRadius);
    }

    // ---- 文字 ----
    QFont f = font();
    f.setPointSize(m_role == Role::Day ? kCellPointSize
                                       : (m_role == Role::Plain ? kNavPointSize
                                                                : kBigCellPointSize));
    f.setBold(m_emphasis || m_today);
    p.setFont(f);
    p.setPen(ink);
    p.drawText(rect(), Qt::AlignCenter, t);
}

// ===========================================================================
// DatePicker
// ===========================================================================
DatePicker::DatePicker(QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(kPanelWidth, kPanelHeight);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_StyledBackground, true);
    buildUi();
    // 与 Keyboard/T9Keyboard 同口径：圆角从窗口层裁掉（QSS 圆角只裁内容不裁窗口）
    setMask(roundedWindowMask(size(), kPanelRadius));
    refreshAll();
}

void DatePicker::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- 顶部标题栏：左选中日期、右 44px 收起钮 ----
    auto* titleBar = new QWidget(this);
    titleBar->setFixedHeight(kTitleHeight);
    auto* titleLay = new QHBoxLayout(titleBar);
    titleLay->setContentsMargins(16, 0, 4, 0);
    titleLay->setSpacing(8);
    m_headerDate = new QLabel(titleBar);
    {
        QFont f = m_headerDate->font();
        f.setPointSize(kTitlePointSize);
        f.setBold(true);
        m_headerDate->setFont(f);
        applyForegroundColor(m_headerDate, titleInkColor());
    }
    titleLay->addWidget(m_headerDate);
    titleLay->addStretch(1);
    m_hideBtn = new PickerButton(PickerButton::Role::Hide, titleBar);
    m_hideBtn->setFixedSize(44, 44);
    m_hideBtn->setText(QString());   // 图标由 paintEvent 画，见下方
    m_hideBtn->setToolTip(QStringLiteral("收起"));
    titleLay->addWidget(m_hideBtn);
    root->addWidget(titleBar);

    // ---- 年月导航栏 ----
    auto* navBar = new QWidget(this);
    navBar->setFixedHeight(kNavHeight);
    auto* navLay = new QHBoxLayout(navBar);
    navLay->setContentsMargins(8, 0, 8, 0);
    navLay->setSpacing(4);
    m_prevYearBtn = new PickerButton(PickerButton::Role::Plain, navBar);
    m_prevMonthBtn = new PickerButton(PickerButton::Role::Plain, navBar);
    m_titleBtn = new PickerButton(PickerButton::Role::Plain, navBar);
    m_nextMonthBtn = new PickerButton(PickerButton::Role::Plain, navBar);
    m_nextYearBtn = new PickerButton(PickerButton::Role::Plain, navBar);
    for (PickerButton* b : {m_prevYearBtn, m_prevMonthBtn, m_nextMonthBtn, m_nextYearBtn})
        b->setFixedSize(44, 44);
    m_prevYearBtn->setText(QStringLiteral("«"));
    m_prevMonthBtn->setText(QStringLiteral("‹"));
    m_nextMonthBtn->setText(QStringLiteral("›"));
    m_nextYearBtn->setText(QStringLiteral("»"));
    m_prevYearBtn->setToolTip(QStringLiteral("上一年"));
    m_prevMonthBtn->setToolTip(QStringLiteral("上一月"));
    m_nextMonthBtn->setToolTip(QStringLiteral("下一月"));
    m_nextYearBtn->setToolTip(QStringLiteral("下一年"));
    navLay->addWidget(m_prevYearBtn);
    navLay->addWidget(m_prevMonthBtn);
    navLay->addWidget(m_titleBtn, 1);
    navLay->addWidget(m_nextMonthBtn);
    navLay->addWidget(m_nextYearBtn);
    root->addWidget(navBar);

    // ---- 三态视图 ----
    m_stack = new QStackedWidget(this);

    // Page 0：日期网格（星期行 + 6×7）
    {
        auto* page = new QWidget;
        auto* vl = new QVBoxLayout(page);
        vl->setContentsMargins(8, 0, 8, 4);
        vl->setSpacing(0);

        auto* weekRow = new QWidget(page);
        weekRow->setFixedHeight(kWeekdayHeight);
        auto* hl = new QHBoxLayout(weekRow);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(0);
        for (int i = 0; i < 7; ++i) {
            auto* lab = new QLabel(weekRow);
            QFont f = lab->font();
            f.setPointSize(kWeekPointSize);
            lab->setFont(f);
            lab->setAlignment(Qt::AlignCenter);
            applyForegroundColor(lab, keyCapSubInkColor());
            hl->addWidget(lab, 1);
            m_weekdayLabels << lab;
        }
        vl->addWidget(weekRow);

        auto* gridHost = new QWidget(page);
        auto* grid = new QGridLayout(gridHost);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(0);
        for (int i = 0; i < 42; ++i) {
            auto* cell = new PickerButton(PickerButton::Role::Day, gridHost);
            cell->setFixedHeight(kDayCellHeight);
            cell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            grid->addWidget(cell, i / 7, i % 7);
            m_dayCells << cell;
        }
        vl->addWidget(gridHost, 1);
        m_stack->addWidget(page);
    }

    // Page 1：选年 4×3
    {
        auto* page = new QWidget;
        auto* grid = new QGridLayout(page);
        grid->setContentsMargins(8, kGridGap, 8, kGridGap);
        grid->setSpacing(kGridGap);
        for (int i = 0; i < 12; ++i) {
            auto* cell = new PickerButton(PickerButton::Role::Year, page);
            grid->addWidget(cell, i / 4, i % 4);
            m_yearCells << cell;
        }
        m_stack->addWidget(page);
    }

    // Page 2：选月 4×3
    {
        auto* page = new QWidget;
        auto* grid = new QGridLayout(page);
        grid->setContentsMargins(8, kGridGap, 8, kGridGap);
        grid->setSpacing(kGridGap);
        for (int i = 0; i < 12; ++i) {
            auto* cell = new PickerButton(PickerButton::Role::Month, page);
            grid->addWidget(cell, i / 4, i % 4);
            m_monthCells << cell;
        }
        m_stack->addWidget(page);
    }
    root->addWidget(m_stack, 1);

    // ---- 底栏：今天 / 取消 ----
    auto* bottomBar = new QWidget(this);
    bottomBar->setFixedHeight(kBottomHeight);
    auto* bl = new QHBoxLayout(bottomBar);
    bl->setContentsMargins(8, 0, 8, 0);
    bl->setSpacing(8);
    m_todayBtn = new PickerButton(PickerButton::Role::Plain, bottomBar);
    m_cancelBtn = new PickerButton(PickerButton::Role::Plain, bottomBar);
    m_todayBtn->setMinimumWidth(104);
    m_cancelBtn->setMinimumWidth(104);
    bl->addWidget(m_todayBtn);
    bl->addStretch(1);
    bl->addWidget(m_cancelBtn);
    root->addWidget(bottomBar);

    // ---- 信号 ----
    connect(m_hideBtn, &QPushButton::clicked, this, &DatePicker::dismissed);
    connect(m_cancelBtn, &QPushButton::clicked, this, &DatePicker::dismissed);
    connect(m_todayBtn, &QPushButton::clicked, this, [this]() {
        const QDate today = QDate::currentDate();
        if (!inRange(today))
            return;   // 今日不在合法范围：不给写（置灰视觉 + 逻辑双保险）
        m_selected = today;
        m_viewYear = today.year();
        m_viewMonth = today.month();
        refreshAll();
        emit datePicked(today);
    });
    connect(m_prevYearBtn, &QPushButton::clicked, this, [this]() {
        if (m_page == Page::Year)
            shiftYearPage(-12);
        else
            shiftMonth(-12);
    });
    connect(m_nextYearBtn, &QPushButton::clicked, this, [this]() {
        if (m_page == Page::Year)
            shiftYearPage(12);
        else
            shiftMonth(12);
    });
    connect(m_prevMonthBtn, &QPushButton::clicked, this, [this]() { shiftMonth(-1); });
    connect(m_nextMonthBtn, &QPushButton::clicked, this, [this]() { shiftMonth(1); });
    connect(m_titleBtn, &QPushButton::clicked, this, [this]() {
        // 日历 → 选年 → 选月 → 日历；选年/选月内部点了就直接落到下一态
        gotoPage(m_page == Page::Day ? Page::Year : (m_page == Page::Year ? Page::Month : Page::Day));
    });
    for (int i = 0; i < int(m_dayCells.size()); ++i) {
        connect(m_dayCells[i], &QPushButton::clicked, this, [this, i]() {
            // 非本月日期：先切月再选中（用户点它就是想去那个月）
            const QDate d = dayCellDate(i);
            if (!inRange(d))
                return;
            m_selected = d;
            m_viewYear = d.year();
            m_viewMonth = d.month();
            refreshAll();
            emit datePicked(d);
        });
    }
    for (int i = 0; i < int(m_yearCells.size()); ++i) {
        connect(m_yearCells[i], &QPushButton::clicked, this, [this, i]() {
            const int y = m_yearPageStart + i;
            if (y < m_minDate.year() || y > m_maxDate.year())
                return;
            m_viewYear = y;
            gotoPage(Page::Month);
        });
    }
    for (int i = 0; i < int(m_monthCells.size()); ++i) {
        connect(m_monthCells[i], &QPushButton::clicked, this, [this, i]() {
            const int y = m_viewYear;
            const int m = i + 1;
            if (y < m_minDate.year() || y > m_maxDate.year())
                return;
            // 只能落在范围内的最近合法月：整月越界（如 minDate 是某月中）时取边界月
            QDate candidate(y, m, 1);
            if (!inRange(candidate)) {
                const QDate lo(m_minDate.year(), m_minDate.month(), 1);
                const QDate hi(m_maxDate.year(), m_maxDate.month(), 1);
                candidate = qBound(lo, candidate, hi);
            }
            m_viewYear = candidate.year();
            m_viewMonth = candidate.month();
            gotoPage(Page::Day);
        });
    }
}

QSize DatePicker::sizeHint() const
{
    return QSize(kPanelWidth, kPanelHeight);
}

void DatePicker::setDateRange(const QDate& minDate, const QDate& maxDate)
{
    m_minDate = minDate.isValid() ? minDate : QDate(1752, 9, 14);
    m_maxDate = maxDate.isValid() ? maxDate : QDate(9999, 12, 31);
    if (m_maxDate < m_minDate)
        qSwap(m_minDate, m_maxDate);
    refreshAll();
}

void DatePicker::setSelectedDate(const QDate& date)
{
    const QDate d = date.isValid() ? date : QDate::currentDate();
    m_selected = inRange(d) ? d : qBound(m_minDate, d, m_maxDate);
    m_viewYear = m_selected.year();
    m_viewMonth = m_selected.month();
    gotoPage(Page::Day);
    refreshAll();
}

void DatePicker::setChineseLocale(bool chinese)
{
    if (m_chinese == chinese)
        return;
    m_chinese = chinese;
    refreshAll();
}

void DatePicker::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = rect();

    // 白底 + 1px 柔和描边（禁 QSS，底色唯一来源）
    p.setPen(Qt::NoPen);
    p.setBrush(keyCapFaceColor());
    p.drawRoundedRect(r, kPanelRadius, kPanelRadius);

    // 顶部主题色标题栏：只有上面两角圆、下面两角方（并集路径拼出来）
    QPainterPath band;
    band.addRoundedRect(QRectF(0, 0, r.width(), kTitleHeight), kPanelRadius, kPanelRadius);
    QPainterPath squareOff;
    squareOff.addRect(QRectF(0, kTitleHeight - kPanelRadius, r.width(), kPanelRadius));
    // fillPath(path, brush) 两版都有；drawPath(path, brush) 是 Qt6 才加的，
    // 裸调会在 Qt5/MSVC 宿主上直接编译失败（C2660）
    p.fillPath(band.united(squareOff), QBrush(brandPanelColor()));

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(panelBorderColor(), 1.0));
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), kPanelRadius, kPanelRadius);
}

void DatePicker::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // 圆角 mask 随尺寸重算；判重防 setMask 触发二次 resize 时反复重入
    const QRegion m = roundedWindowMask(size(), kPanelRadius);
    if (mask() != m)
        setMask(m);
}

bool DatePicker::inRange(const QDate& d) const
{
    return d.isValid() && d >= m_minDate && d <= m_maxDate;
}

QDate DatePicker::dayCellDate(int index) const
{
    const QDate first(m_viewYear, m_viewMonth, 1);
    const QDate gridStart = first.addDays(-(first.dayOfWeek() - 1));
    return gridStart.addDays(index);
}

void DatePicker::gotoPage(Page page)
{
    m_page = page;
    m_stack->setCurrentIndex(int(page));
    refreshTexts();
}

void DatePicker::shiftMonth(int months)
{
    // 基准一律 QDate(y, m, 1)：拿选定日号构造会在 1/31 → 2 月时产出无效日期
    QDate base(m_viewYear, m_viewMonth, 1);
    base = base.addMonths(months);
    if (!base.isValid()) {
        // addMonths 溢出（极端范围）时退回边界
        base = months < 0 ? QDate(m_minDate.year(), m_minDate.month(), 1)
                          : QDate(m_maxDate.year(), m_maxDate.month(), 1);
    }
    m_viewYear = base.year();
    m_viewMonth = base.month();
    refreshAll();
}

void DatePicker::shiftYearPage(int years)
{
    const int minYear = m_minDate.year();
    const int maxYear = m_maxDate.year();
    int start = m_yearPageStart + years;
    // 页内 12 年必须完整落在范围内：贴住任一边界
    if (start < minYear)
        start = minYear;
    if (start + 11 > maxYear)
        start = qMax(minYear, maxYear - 11);
    m_yearPageStart = start;
    refreshAll();
}

void DatePicker::refreshAll()
{
    if (m_viewYear == 0 || m_viewMonth == 0) {
        m_viewYear = m_selected.year();
        m_viewMonth = m_selected.month();
    }
    if (m_yearPageStart == 0)
        m_yearPageStart = m_viewYear - (m_viewYear % 12);   // 12 年一页，页界对齐
    refreshTexts();
    refreshDayGrid();
    refreshYearGrid();
    refreshMonthGrid();
    refreshHeader();
}

void DatePicker::refreshTexts()
{
    static const QString zhMonths[12] = {
        QStringLiteral("1月"), QStringLiteral("2月"), QStringLiteral("3月"),
        QStringLiteral("4月"), QStringLiteral("5月"), QStringLiteral("6月"),
        QStringLiteral("7月"), QStringLiteral("8月"), QStringLiteral("9月"),
        QStringLiteral("10月"), QStringLiteral("11月"), QStringLiteral("12月")};
    static const QString enMonths[12] = {
        QStringLiteral("Jan"), QStringLiteral("Feb"), QStringLiteral("Mar"),
        QStringLiteral("Apr"), QStringLiteral("May"), QStringLiteral("Jun"),
        QStringLiteral("Jul"), QStringLiteral("Aug"), QStringLiteral("Sep"),
        QStringLiteral("Oct"), QStringLiteral("Nov"), QStringLiteral("Dec")};

    // 星期行：周一开头（与 42 格起始算法 dayOfWeek()-1 对齐）
    static const QString zhDays[7] = {
        QStringLiteral("一"), QStringLiteral("二"), QStringLiteral("三"), QStringLiteral("四"),
        QStringLiteral("五"), QStringLiteral("六"), QStringLiteral("日")};
    static const QString enDays[7] = {
        QStringLiteral("Mon"), QStringLiteral("Tue"), QStringLiteral("Wed"),
        QStringLiteral("Thu"), QStringLiteral("Fri"), QStringLiteral("Sat"),
        QStringLiteral("Sun")};
    for (int i = 0; i < 7; ++i)
        m_weekdayLabels[i]->setText(m_chinese ? zhDays[i] : enDays[i]);

    for (int i = 0; i < 12; ++i)
        m_monthCells[i]->setText(m_chinese ? zhMonths[i] : enMonths[i]);

    m_todayBtn->setText(m_chinese ? QStringLiteral("今天") : QStringLiteral("Today"));
    m_cancelBtn->setText(m_chinese ? QStringLiteral("取消") : QStringLiteral("Cancel"));

    // 标题按钮：日历态显年月、选年态显页区间、选月态显年
    if (m_page == Page::Year) {
        m_titleBtn->setText(QStringLiteral("%1 - %2").arg(m_yearPageStart)
                                .arg(m_yearPageStart + 11));
    } else if (m_page == Page::Month) {
        m_titleBtn->setText(m_chinese ? QStringLiteral("%1年").arg(m_viewYear)
                                      : QString::number(m_viewYear));
    } else {
        m_titleBtn->setText(m_chinese
            ? QStringLiteral("%1年 %2月").arg(m_viewYear).arg(m_viewMonth)
            : QStringLiteral("%1 %2").arg(enMonths[qBound(0, m_viewMonth - 1, 11)])
                                      .arg(m_viewYear));
    }

    // 选年态下翻年钮改作整页翻（±12 年），语义与图标一致
    const bool yearPage = (m_page == Page::Year);
    m_prevYearBtn->setText(QStringLiteral("«"));
    m_nextYearBtn->setText(QStringLiteral("»"));
    m_prevYearBtn->setToolTip(yearPage ? QStringLiteral("上 12 年")
                                       : QStringLiteral("上一年"));
    m_nextYearBtn->setToolTip(yearPage ? QStringLiteral("下 12 年")
                                       : QStringLiteral("下一年"));
    m_prevYearBtn->setEnabled(yearPage || m_viewYear > m_minDate.year());
    m_nextYearBtn->setEnabled(yearPage || m_viewYear < m_maxDate.year());
    // 翻月钮只在日历态有意义（选年/选月态留着会让用户误以为能翻月份）
    const QDate viewFirst(m_viewYear, m_viewMonth, 1);
    m_prevMonthBtn->setEnabled(!yearPage && m_page == Page::Day
                               && viewFirst > QDate(m_minDate.year(), m_minDate.month(), 1));
    m_nextMonthBtn->setEnabled(!yearPage && m_page == Page::Day
                               && viewFirst < QDate(m_maxDate.year(), m_maxDate.month(), 1));
    m_titleBtn->setToolTip(m_chinese ? QStringLiteral("切换年 / 月选择")
                                     : QStringLiteral("Switch year / month picker"));
    m_todayBtn->setEnabled(inRange(QDate::currentDate()));
}

void DatePicker::refreshHeader()
{
    m_headerDate->setText(m_chinese
        ? QStringLiteral("%1年%2月%3日").arg(m_selected.year())
              .arg(m_selected.month()).arg(m_selected.day())
        : m_selected.toString(QStringLiteral("yyyy-MM-dd")));
}

void DatePicker::refreshDayGrid()
{
    // 42 格起始日：视图月 1 号回退到本周周一（dayOfWeek 1=周一）
    const QDate first(m_viewYear, m_viewMonth, 1);
    const int offset = first.dayOfWeek() - 1;
    const QDate gridStart = first.addDays(-offset);

    const QDate today = QDate::currentDate();
    for (int i = 0; i < 42; ++i) {
        const QDate d = gridStart.addDays(i);
        PickerButton* cell = m_dayCells[i];
        cell->setText(QString::number(d.day()));
        cell->setEnabled(inRange(d));
        cell->setEmphasis(d == m_selected);
        cell->setToday(d == today);
        cell->setMuted(d.month() != m_viewMonth || d.year() != m_viewYear);
    }
}

void DatePicker::refreshYearGrid()
{
    for (int i = 0; i < 12; ++i) {
        const int y = m_yearPageStart + i;
        PickerButton* cell = m_yearCells[i];
        cell->setText(QString::number(y));
        cell->setEnabled(y >= m_minDate.year() && y <= m_maxDate.year());
        cell->setEmphasis(y == m_viewYear);
        cell->setToday(y == QDate::currentDate().year());
    }
}

void DatePicker::refreshMonthGrid()
{
    for (int i = 0; i < 12; ++i) {
        const int m = i + 1;
        PickerButton* cell = m_monthCells[i];
        // 整月是否与合法范围有交集：无交集的月置灰（如 minDate 是 6 月 15 日，6 月仍可选）
        const QDate monthFirst(m_viewYear, m, 1);
        const QDate monthLast(m_viewYear, m, QDate(m_viewYear, m, 1).daysInMonth());
        const bool usable = monthFirst <= m_maxDate && monthLast >= m_minDate;
        cell->setEnabled(usable);
        cell->setEmphasis(m == m_viewMonth);
        cell->setToday(m == QDate::currentDate().month()
                       && m_viewYear == QDate::currentDate().year());
    }
}

}  // namespace AeaQt
