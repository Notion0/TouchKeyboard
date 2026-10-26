#include "Keyboard.h"
#include "T9Keyboard.h"
#include "NumberKeyboard.h"
#include "InputManager.h"
#include <QApplication>
#include <QAbstractSpinBox>
#include <QPointer>
#include <QTimer>

/**
 * 单例获取
 */
InputManager* InputManager::Instance()
{
    static InputManager manager;
    return &manager;
}

/**
 * 构造函数
 */
InputManager::InputManager(QObject *parent) : QObject(parent)
{
}

/**
 * 初始化键盘管理器
 * - 创建全键盘与数字小键盘（九宫格按需创建，见 ensureT9Keyboard）
 * - 设置窗口属性（无焦点、置顶等）
 * - 连接信号
 * - 安装全局事件过滤器
 */
void InputManager::Init(QWidget *parent)
{
    mainWidget = parent;

    // 必须挂到主窗口（Qt6 移植修正）：无父 Tool 窗在 Weston/X11 下每次 show
    // 都被当作新 top-level surface 抢激活权——主窗失活触发 focusChanged(nullptr)
    // 收起键盘，焦点回输入框又弹出，形成 show/hide 拉锯（界面一直闪、位置飘忽）。
    // 挂父后键盘是主窗口拥有的工具窗，显示不改变应用激活态，拉锯根除
    keyboard = new AeaQt::Keyboard(parent);
    numberKeyboard = new AeaQt::NumberKeyboard(parent);
    prepareKeyboard(keyboard);
    prepareKeyboard(numberKeyboard);

    // Init 前已选了九宫格（setTextLayout 先于 Init 调用）：现在建
    if (m_layout == T9)
        ensureT9Keyboard();

    // 焦点变化信号
    connect(qApp, &QApplication::focusChanged, this, &InputManager::focusChanged);

    // 全局事件过滤（用于捕获点击）
    qApp->installEventFilter(this);
}

void InputManager::prepareKeyboard(AeaQt::AbstractKeyboard *kb)
{
    // 作主窗口的普通子控件（不是顶层 Tool 窗）： Weston/X11 下顶层工具窗
    // 每次映射都被抢走激活权——主窗失活触发 focusChanged(nullptr) 收起，
    // 主窗重新激活又还原输入框焦点再弹，形成 show/hide 拉锯（界面一直闪）。
    // 子控件显示不触碰激活态，从结构上根除；代价是键盘不浮出主窗口边界
    // （PUPSIT 单全屏窗口，无此场景）
    kb->setFocusPolicy(Qt::NoFocus);
    for (auto child : kb->findChildren<QWidget*>())
        child->setFocusPolicy(Qt::NoFocus);
    kb->hide();

    // 布局切换键：排队执行——切换会隐藏发出信号的键盘本身，不在按键回调栈里做
    connect(kb, &AeaQt::AbstractKeyboard::layoutSwitchRequested,
            this, &InputManager::toggleTextLayout, Qt::QueuedConnection);
}

void InputManager::ensureT9Keyboard()
{
    if (t9Keyboard || !mainWidget)
        return;
    t9Keyboard = new AeaQt::T9Keyboard(mainWidget);
    prepareKeyboard(t9Keyboard);
    if (!m_extraWords.isEmpty())
        t9Keyboard->setExtraWords(m_extraWords);
}

AeaQt::AbstractKeyboard *InputManager::textKeyboard()
{
    if (m_layout == T9) {
        ensureT9Keyboard();
        if (t9Keyboard)
            return t9Keyboard;
    }
    return keyboard;
}

/**
 * 设置文本键盘布局：若文本键盘正显示，把中英态带过去并对当前输入框重弹
 */
void InputManager::setTextLayout(TextLayout layout)
{
    if (m_layout == layout)
        return;

    AeaQt::AbstractKeyboard *old = mainWidget ? textKeyboard() : nullptr;
    const bool wasShown = old && old->isVisible();
    const bool chinese = old && old->isChineseMode();

    m_layout = layout;
    emit textLayoutChanged(m_layout);
    if (!mainWidget)
        return;   // Init 前：只记布局，Init 时按它创建

    AeaQt::AbstractKeyboard *now = textKeyboard();
    if (old && now != old)
        now->setChineseMode(chinese);   // 英文-only 字段下请求中文会被键盘自行忽略

    if (wasShown && currentInput) {
        QWidget *input = currentInput;
        old->hide();
        currentInput = nullptr;   // 绕过 showKeyboard 的「同框同键盘」早退
        showKeyboard(input);
    }
}

void InputManager::toggleTextLayout()
{
    setTextLayout(m_layout == T9 ? Qwerty : T9);
}

/**
 * 宿主注入业务词：九宫格已建则直接转发，否则缓存到创建时
 */
void InputManager::setExtraWords(const QStringList &words)
{
    m_extraWords = words;
    if (t9Keyboard)
        t9Keyboard->setExtraWords(words);
}

/**
 * 该控件是否属于任一键盘（点击/焦点落在键盘上时不当作外部点击）
 */
bool InputManager::isKeyboardWidget(QWidget* w) const
{
    if (!w)
        return false;

    const AeaQt::AbstractKeyboard *kbs[] = {keyboard, t9Keyboard, numberKeyboard};
    for (const AeaQt::AbstractKeyboard *kb : kbs) {
        if (kb && (w == kb || kb->isAncestorOf(w)))
            return true;
    }
    return false;
}

/**
 * 两个键盘一并收起
 */
void InputManager::hideKeyboards()
{
    keyboard->hide();
    if (t9Keyboard)
        t9Keyboard->hide();
    numberKeyboard->hide();
    currentInput = nullptr;
}

/**
 * 显示键盘（核心函数）
 * - 按 numericInput 属性选全键盘/小键盘
 * - 负责定位 + 边界处理 + 显示
 */
void InputManager::showKeyboard(QWidget* now)
{
    if (!now) return;

    // QSpinBox/QDoubleSpinBox 的焦点实际落在内嵌行编辑（qt_spinbox_lineedit）上，
    // numericInput 属性写在外层 spinbox——沿 QAbstractSpinBox 父链补查一层拿到判据，
    // 否则所有数值 spinbox 一律弹全键盘
    const QWidget *holder = now;
    if (!holder->property("numericInput").isValid() &&
        qobject_cast<const QAbstractSpinBox*>(holder->parentWidget()))
        holder = holder->parentWidget();

    AeaQt::AbstractKeyboard *text = textKeyboard();   // 26 键或九宫格（按当前布局）
    AeaQt::AbstractKeyboard *target = holder->property("numericInput").toBool()
        ? static_cast<AeaQt::AbstractKeyboard*>(numberKeyboard)
        : text;

    // 英文-only 字段（密码等仅允许 ASCII）：文本键盘停靠英文态，且中/英切换键置灰。
    // 不禁的话触摸键盘切中文后，拼音候选能点选却过不了字段的 validator——
    // 表现为"中文能显示但不能选中写入"。数字小键盘本就无中文态，无需处理
    if (target == text)
        text->setEnglishOnly(holder->property("asciiOnly").toBool());

    // 如果已经是当前输入框且目标键盘已显示，则不重复处理
    if (target->isVisible() && currentInput == now)
        return;

    currentInput = now;

    // 只显示目标键盘，另一个收起
    keyboard->setVisible(target == keyboard);
    if (t9Keyboard)
        t9Keyboard->setVisible(target == t9Keyboard);
    numberKeyboard->setVisible(target == numberKeyboard);

    // 数字小键盘开面板前整选数值段（口径见 AbstractKeyboard.h selectAllNumericContent）：
    // 第一个按键即替换旧值，退格一次删完整段数字——不整选时按键按 QLineEdit 光标语义
    // 插入，光标又停在数值末尾/后缀之前，表现为「数字越拼越长」
    // 排到当前事件之后：点击弹出时本函数在 eventFilter（mousePress 之前）里被调，
    // 同步整选会被随后 QLineEdit::mousePressEvent 重置成光标
    if (target == numberKeyboard) {
        QPointer<QWidget> w(now);
        QTimer::singleShot(0, this, [w] { selectAllNumericContent(w); });
    }

    // 获取输入框下方位置（全局坐标）
    QPoint globalPos = now->mapToGlobal(QPoint(0, now->height()));

    target->adjustSize();
    QSize keyboardSize = target->size();

    // 键盘是主窗口子控件：换算到主窗口本地坐标，边界按主窗矩形钳制
    // （子控件不可能绕出主窗，无需再管屏幕）
    const QPoint hostOrigin = mainWidget->mapToGlobal(QPoint(0, 0));
    const QRect hostRect = mainWidget->rect();

    int x = globalPos.x() - hostOrigin.x();
    int y = globalPos.y() - hostOrigin.y();

    // ===== 右侧越界 =====
    if (x + keyboardSize.width() > hostRect.right())
        x = hostRect.right() - keyboardSize.width();

    // ===== 左侧越界 =====
    if (x < hostRect.left())
        x = hostRect.left();

    // ===== 底部越界 -> 放到输入框上方 =====
    if (y + keyboardSize.height() > hostRect.bottom())
        y = now->mapToGlobal(QPoint(0, 0)).y() - hostOrigin.y() - keyboardSize.height();

    // ===== 顶部越界 =====
    if (y < hostRect.top())
        y = hostRect.top();

    target->move(x, y);
    target->show();
    target->raise();
}

/**
 * 焦点变化处理
 * - 当输入框获得焦点 → 显示键盘
 * - 当焦点丢失 → 隐藏键盘
 */
void InputManager::focusChanged(QWidget *old, QWidget *now)
{
    Q_UNUSED(old);

    // 点击的是键盘本身 → 忽略
    if (isKeyboardWidget(now)) {
        return;
    }

    // 没有焦点 → 隐藏
    if (!now) {
        hideKeyboards();
        return;
    }

    // 输入控件 → 显示键盘
    if (qobject_cast<QLineEdit*>(now) ||
        qobject_cast<QTextEdit*>(now)
        )
    {
        showKeyboard(now);
    }
    else
    {
        // 其他控件 → 隐藏
        hideKeyboards();
    }
}

/**
 * 全局事件过滤器（关键修复点）
 * - 监听鼠标点击
 * - 点击输入框：强制弹出键盘（即使焦点没变）
 * - 点击其他区域：隐藏键盘
 */
bool InputManager::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        QWidget *widget = qobject_cast<QWidget*>(obj);

        if (!widget)
            return QObject::eventFilter(obj, event);

        // 点击键盘 → 不处理
        if (isKeyboardWidget(widget))
        {
            return QObject::eventFilter(obj, event);
        }

        // ⭐ 点击输入框 → 强制弹出（修复关键）
        if (widget->inherits("QLineEdit") ||
            widget->inherits("QTextEdit") ||
            widget->inherits("QPlainTextEdit"))
        {
            // 已为该框弹出的小键盘（可见且 currentInput 正是本框）：showKeyboard 对
            // 「同输入+同键盘」直接早退，这里补一次整选——重复点击该框即整段重输。
            // 整选排到本次鼠标事件之后：QLineEdit 自己的 mousePress 会把选区重置成光标
            const bool reselectForOpen = numberKeyboard->isVisible()
                                         && currentInput == widget;
            showKeyboard(widget);
            if (reselectForOpen) {
                QPointer<QWidget> w(widget);
                QTimer::singleShot(0, this, [w] { selectAllNumericContent(w); });
            }
            return QObject::eventFilter(obj, event);
        }

        // 点击其他区域 → 隐藏
        hideKeyboards();
    }

    return QObject::eventFilter(obj, event);
}
