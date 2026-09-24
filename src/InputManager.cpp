#include "Keyboard.h"
#include "NumberKeyboard.h"
#include "InputManager.h"
#include <QApplication>

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
 * - 创建全键盘与数字小键盘
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

    AeaQt::AbstractKeyboard *kbs[] = {keyboard, numberKeyboard};
    for (AeaQt::AbstractKeyboard *kb : kbs) {
        // 作主窗口的普通子控件（不是顶层 Tool 窗）： Weston/X11 下顶层工具窗
        // 每次映射都被抢走激活权——主窗失活触发 focusChanged(nullptr) 收起，
        // 主窗重新激活又还原输入框焦点再弹，形成 show/hide 拉锯（界面一直闪）。
        // 子控件显示不触碰激活态，从结构上根除；代价是键盘不浮出主窗口边界
        // （PUPSIT 单全屏窗口，无此场景）
        kb->setFocusPolicy(Qt::NoFocus);
        for (auto child : kb->findChildren<QWidget*>())
            child->setFocusPolicy(Qt::NoFocus);
        kb->hide();
    }

    // 全键盘输入信号（数字小键盘自带 QKeyEvent 通道，无需接线）
    connect(keyboard, &AeaQt::Keyboard::pressedChanged, this, &InputManager::onKeyboardInput);
    // 焦点变化信号
    connect(qApp, SIGNAL(focusChanged(QWidget*,QWidget*)),this, SLOT(focusChanged(QWidget*,QWidget*)));

    // 全局事件过滤（用于捕获点击）
    qApp->installEventFilter(this);
}

/**
 * 该控件是否属于任一键盘（点击/焦点落在键盘上时不当作外部点击）
 */
bool InputManager::isKeyboardWidget(QWidget* w) const
{
    if (!w)
        return false;

    return w == keyboard || keyboard->isAncestorOf(w) ||
           w == numberKeyboard || numberKeyboard->isAncestorOf(w);
}

/**
 * 两个键盘一并收起
 */
void InputManager::hideKeyboards()
{
    keyboard->hide();
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

    AeaQt::AbstractKeyboard *target = now->property("numericInput").toBool()
        ? static_cast<AeaQt::AbstractKeyboard*>(numberKeyboard)
        : static_cast<AeaQt::AbstractKeyboard*>(keyboard);

    // 如果已经是当前输入框且目标键盘已显示，则不重复处理
    if (target->isVisible() && currentInput == now)
        return;

    currentInput = now;

    // 只显示目标键盘，另一个收起
    keyboard->setVisible(target == keyboard);
    numberKeyboard->setVisible(target == numberKeyboard);

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
 * 键盘输入处理（全键盘通道）
 * - 将虚拟键盘输入写入当前控件
 */
void InputManager::onKeyboardInput(int code, QString text)
{
    Q_UNUSED(code);

    if (!currentInput) return;

    // 防止焦点丢失
    if (!currentInput->hasFocus())
        currentInput->setFocus();

    if (QLineEdit *edit = qobject_cast<QLineEdit*>(currentInput))
    {
        edit->insert(text);
    }
    else if (QTextEdit *edit = qobject_cast<QTextEdit*>(currentInput))
    {
        edit->insertPlainText(text);
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
            showKeyboard(widget);
            return QObject::eventFilter(obj, event);
        }

        // 点击其他区域 → 隐藏
        hideKeyboards();
    }

    return QObject::eventFilter(obj, event);
}
