// Keyboard/DatePicker.h
// 工业 HMI 触屏日期选择器：零 QSS、全自绘，青色圆角风格与三枚键盘同族。
// 三态视图（日历 42 格 / 选年 4×3 / 选月 4×3）+ 顶部自绘矢量翻年翻月 +
// 「今天 / 取消」底栏；中文默认、可随宿主切英文。
//
// ⚠ 接入方式：不建顶层窗（Wayland 下顶层 Tool 窗每次映射都被抢激活权，见
//   InputManager.h 顶部注释），由 InputManager 挂成 mainWidget 的普通子控件，
//   圆角用 roundedWindowMask 从窗口层裁掉。点击目标全部 ≥44px，全部 NoFocus
//   （焦点一变 InputManager 的 focusChanged 会把面板收起）。
#ifndef AEA_DATEPICKER_H
#define AEA_DATEPICKER_H

#include <QDate>
#include <QPushButton>
#include <QWidget>

class QLabel;
class QStackedWidget;

namespace AeaQt {

/// 自绘大触控格按钮（日期格 / 年格 / 月格 / 导航与底栏文字钮共用同一类，不新增类）。
/// 只覆写 paintEvent：
///  - 禁 setAutoFillBackground(true)：Qt 会在画圆角前铺矩形底，四角被填方；
///  - 必设 Qt::NoFocus：点击抢焦点会触发 InputManager::focusChanged 把自己收起；
///  - 不用 QEnterEvent（Qt5/6 签名差异坑），按压/悬停用 isDown()/underMouse() 判。
class PickerButton final : public QPushButton {
public:
    enum class Role { Day, Year, Month, Plain, Hide };

    explicit PickerButton(Role role, QWidget* parent = nullptr);

    /// @brief 主强调态：选中日 / 当前年 / 当前月（主题色实心块 + 白粗字）
    void setEmphasis(bool on);
    /// @brief 今日标记（非选中时画主题色圆环 + 主题色粗字）
    void setToday(bool on);
    /// @brief 弱化态：非本月的日期（浅灰，仍可点——点了自动切月）
    void setMuted(bool on);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    Role m_role;
    bool m_emphasis = false;
    bool m_today = false;
    bool m_muted = false;
};

/// 触屏日期选择器面板（固定 460×444）。选中日期发 datePicked，收起发 dismissed。
class DatePicker final : public QWidget {
    Q_OBJECT
public:
    explicit DatePicker(QWidget* parent = nullptr);

    /// @brief 合法日期范围（默认取 QDateEdit 的极值；越界日期格置灰不可点）
    void setDateRange(const QDate& minDate, const QDate& maxDate);

    /// @brief 设当前选中日期，并把视图切到该日期所在月（每次弹出都调，状态可预期）
    void setSelectedDate(const QDate& date);

    /// @brief 中英文文案（默认中文）；宿主切换界面语言时调一次
    void setChineseLocale(bool chinese);

    QSize sizeHint() const override;

signals:
    /// @brief 选中一个合法日期（含「今天」）
    void datePicked(const QDate& date);
    /// @brief 用户收起面板（右上收起钮 / 取消 / 点击面板外）
    void dismissed();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    enum class Page { Day, Year, Month };

    void buildUi();
    /// @brief 第 index 个日期格对应的日期（含非本月的补格）
    QDate dayCellDate(int index) const;
    void refreshAll();
    void refreshDayGrid();
    void refreshYearGrid();
    void refreshMonthGrid();
    void refreshTexts();
    void refreshHeader();
    void gotoPage(Page page);
    /// @brief 视图月前后移动（一律以 QDate(y, m, 1) 为基准，绝不用选定日号构造——
    ///        1 月 31 日往前一个月会产出无效 QDate）
    void shiftMonth(int months);
    /// @brief 选年视图整页翻（±12 年）
    void shiftYearPage(int years);
    bool inRange(const QDate& d) const;

    QLabel*        m_headerDate = nullptr;
    PickerButton*  m_hideBtn = nullptr;
    PickerButton*  m_prevYearBtn = nullptr;
    PickerButton*  m_prevMonthBtn = nullptr;
    PickerButton*  m_titleBtn = nullptr;
    PickerButton*  m_nextMonthBtn = nullptr;
    PickerButton*  m_nextYearBtn = nullptr;
    QStackedWidget* m_stack = nullptr;
    QVector<QLabel*>       m_weekdayLabels;
    QVector<PickerButton*> m_dayCells;
    QVector<PickerButton*> m_yearCells;
    QVector<PickerButton*> m_monthCells;
    PickerButton*  m_todayBtn = nullptr;
    PickerButton*  m_cancelBtn = nullptr;

    QDate m_minDate = QDate(1752, 9, 14);       ///< 同 QDateEdit 默认极值
    QDate m_maxDate = QDate(9999, 12, 31);
    QDate m_selected = QDate::currentDate();
    int   m_viewYear = 0;      ///< 视图所在年（0 = 尚未初始化）
    int   m_viewMonth = 0;     ///< 视图所在月 1..12
    int   m_yearPageStart = 0; ///< 选年视图首页年份
    Page  m_page = Page::Day;
    bool  m_chinese = true;
};

}  // namespace AeaQt

#endif // AEA_DATEPICKER_H
