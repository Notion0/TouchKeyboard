# TouchKeyboard —— 触摸键盘模板

> 仓库：https://github.com/Notion0/TouchKeyboard （Qt Widgets 5.15 / 6.x 双版本 / C++11 / MSVC·GCC 通吃）

文本键盘两种布局并存（26 键全键盘 / 九宫格 T9，运行期一键切换）+ 数字小键盘 + 全局 InputManager。Qt Widgets（5.15 与 6.x 双版本编译验证）、C++11、MSVC(2019)/GCC 通吃。
从 PressureMonitor 项目抽出（2026-09-24），两键盘形态以生产巡检后的稳定版为准；现同时供 PressureMonitor（Qt 5.15 线）与 PUPSIT（Qt 6 线）两个宿主使用。

## 30 秒接入

1. 宿主 `.pro` 加一行（模板放宿主同级目录 `../TouchKeyboard` 时最省事；也可绝对路径）：
   ```qmake
   include(../TouchKeyboard/TouchKeyboard.pri)
   ```
2. 主窗口创建后调用一次：
   ```cpp
   #include "InputManager.h"          // 模板 src/ 已在 INCLUDEPATH，无需带目录前缀
   InputManager::Instance()->Init(this);   // this = 键盘的宿主父窗口（通常是主窗口）
   ```
   ⚠ **`Init(parent)` 的 parent 即键盘宿主窗口**：两个键盘是该窗口的**普通子控件**
   （不是顶层 Tool 窗）。Weston/X11 下顶层工具窗每次映射都被抢激活权，主窗失活 →
   `focusChanged(nullptr)` 收起 → 主窗重新激活还原输入框焦点 → 再弹，形成无限
   show/hide 拉锯（界面一直闪、键盘位置飘忽）；挂子控件从结构上根除。代价：键盘
   不浮出宿主窗口边界（宿主是全屏主窗口时无感知）。
3. 给**纯数字**输入框打动态属性（决定弹小键盘还是全键盘）：
   - `.ui` 里：
     ```xml
     <property name="numericInput" stdset="0">
      <bool>true</bool>
     </property>
     ```
   - 代码里：`edit->setProperty("numericInput", true);`
   - ⚠ **`stdset="0"` 不可省**：uic 对无此标记的属性会按"标准属性"直接生成 `setNumericInput(true)` 编译失败（踩过）；带上才生成 `setProperty()` 动态属性调用。
   不标的输入框（用户名/批号/标题等）弹文本键盘。
4. （可选）默认用九宫格：`InputManager::Instance()->setTextLayout(InputManager::T9);`
   Init 前后都可调。用户在键盘上点「九宫格 / 全键盘」键也会切换，`textLayout()` /
   `textLayoutChanged` 可读到最后选择（不持久化，要跨重启记忆由宿主自己存）。

## 九宫格 T9（2026-10-26 自 aerocount 分叉线合回）

- 版式 672×360：拼音组合栏 + 候选栏 + 3×3 数字键；左列符号/标点/123，右列退格/清空/确认；
  末行「中/英 | 0 空格 | 全键盘」。全部自绘，不用 QSS。
- 四种模式：中文九宫格拼音（组合栏点选锁定拼音、`1` 分词、选词后剩余数字继续出候选、
  中文态 `0` 上屏首选）/ 英文（按数字键后组合栏出 a b c A B C 直选 + 候选栏英文词）/ 123 / 符号页。
- 英文词来源：学过的词（QSettings `T9Keyboard/learnedWords`，密码框不学）> 宿主业务词
  （`InputManager::setExtraWords()` 或输入框 `t9Words` 动态属性，QStringList）> 内置常用词。
- 密码框（`echoMode==Password`）：键盘顶部回显真实已输内容。
- `asciiOnly` 输入框：两种布局都停靠英文态，中/英键置灰。
- 词库：复用同三字典，**后台 QThread 加载**，加载完前候选栏显示「词库加载中…」。九宫格
  **按需创建**——首次切到九宫格才建，从不用九宫格的宿主不付这份内存。
- ⚠ 后台线程用 `QThread` 子类，不要改用 `QThread::create`/`std::future`/`std::async`：
  aerocount 板端 libstdc++（GCC 10.3）缺 `CXXABI_1.3.13`，启动即崩（2026-10-22 实测）。
- 单测：`tests/t9/`（`qmake && make && ./tst_t9engine`）。

## 行为参考

| | 全键盘 | 数字小键盘 |
|---|---|---|
| 大小 | 990×300（标题栏 40 + 4 行键） | 330×355（标题栏 34 + 4 行 65px + 动作行 65px） |
| 弹出条件 | 焦点进入任意 QLineEdit/QTextEdit | 同上且 `numericInput==true`（两键盘互斥） |
| 弹出时 | — | **整选数值段**：首键即替换旧值；spinbox 只选数字不含单位；再点同一框重新整选（自 aerocount 合回） |
| 提交 | **回车**键（editingFinished + 失焦 + 收起） | **确认**键（同语义；小键盘无回车键必须有） |
| 清空 | 底行**清空**键（清拼音缓冲 + 清空输入框） | 底行**清空**键（清空输入框） |
| 收起 | 点击非输入区 / 失焦 / 标题栏关闭钮 | 点击非输入区 / 失焦 / 确认 |
| 中文 | 中/英切换（**空格左侧键帽「汉/en」大字状态标识**点击即切）+ 拼音候选行（需字典，见下） | 无 |
| 位置 | 输入框下方，越界在**宿主窗口矩形内**上翻/钳制 | 同 |

按键统一走 `AbstractKeyboard::onKeyPressed` 的 QKeyEvent 通道发到焦点控件；两个键盘是宿主
窗口的无焦点子控件（子控件本身不抢焦点），点键不抢焦点、不改变应用激活态。

## 中文输入（内置能力，非可选项）

拼音输入是键盘的**固有能力**——三字典（单字 2 万 / 词组 41 万 / 谷歌大字典 6.5 万，
共约 9.6MB）已随模板 qrc 编入（`resources/dicts/`），宿主 `include(.pri)` 即得完整
中文输入，**不需要、也不存在「不带中文」的接入形态**。

字典资源路径与 `Keyboard.cpp` 的 `TOUCHKBD_*` 宏默认值逐字一致（无需任何配置）：

| 宏（仅用于覆盖为自定义字典） | 默认路径 | 内容 |
|---|---|---|
| `TOUCHKBD_PINYIN_DICT` | `:/ChineseLib/pinyin.txt` | 单字拼音 |
| `TOUCHKBD_PINYIN_PHRASE_DICT` | `:/ChinesePhraseLib/pinyin_phrase.txt` | 词组（41 万条，去声调+ü→v） |
| `TOUCHKBD_GOOGLE_DICT` | `:/GoogleChineseLib/rawdict_utf16_65105_freq_sort.txt` | 谷歌大字典（词频排序） |

扁平路径由模板 qrc 的 `alias` 保证（`dicts/` 下实体文件映射掉 `/dicts` 一节）——**alias
不可省**：漏写时真实路径多出 `/dicts`，三个字典 open 全败、候选栏只剩原始拼音，
且失败提示是 qDebug（发布构建 `QT_NO_DEBUG_OUTPUT` 下静默）。PUPSIT(2026-09-29)与
PressureMonitor_noflow(2026-10-09)两次现场反馈同一根因；`Keyboard.cpp` 的
`openDictFile()` 另做扁平/嵌套双路径兜底 + qWarning 总检，不再依赖单一配置正确。

## 中/英状态标识

全键盘**空格左侧的那个键位**就是中/英状态标识：键帽常显 **中文态「汉/en」/ 英文态「en/汉」**
（当前态在前、目标态在后，30px 加粗，普通键帽 26px，明显更大），**点击即切换**。该键本来就是中/英切换键——
状态显示与切换功能合并在同一键上，不占额外键位也不丢功能；候选区只在中文态可见
（英文态收起，高度归 0）。英文态字母键直接上屏，中文态字母键拼拼音出候选、点候选
上屏；离开中文态（或键盘收起）时未提交的拼音缓冲与候选一并丢弃，不会串到下一个
输入框。

实现要点（`Keyboard.cpp`）：`resizeButton()` → `fitKeyFont()` 给该键设 `objectName=langSwitchKey` 并下发
`AeaQt--KeyButton#langSwitchKey { font-size: 30px; font-weight: bold; }`（按实际键宽放不下时逐级缩字，
「九宫格」键同理走 `#layoutSwitchKey`，基准 26px）——ID 选择器
优先级高于 KeyButton 默认键帽 QSS（`DEFAULT_STYLE_SHEET` 的 26px），故只有这一个键
变大字（不能用 `setFont`：QSS 设了 font-size 会覆盖控件字体）；键帽文本由
`m_isChinese` 驱动、收在 `updateLangState()` 单一出处（不走 modes 循环——单字循环态
表达不了「汉/en」组合语义）；底行份数 ?123:九宫格:中英:空格:回车:清空 = 12:20:20:26:22:22（2026-10-26：
宿主钳到 672 宽时原 14 份放不下「九宫格」与「en/汉」，各提到 20 份并加缩字兜底）。
另注意：切符号页（`?123`）不会动这个键（`switchSpecialChar()` 显式排除），否则键帽
被打回英文态而 `m_isChinese` 仍是中文，状态显示与实际中英态相反。

## 换肤 / 换品牌

改 `src/AbstractKeyboard.h` 一处，两个键盘同时生效：

- `brandLogoText()` —— 标题栏 logo 文本（默认 `WOWA-无涯`，全键盘 15pt / 小键盘 15pt 白粗体）
- `brandPanelColor()` —— 面板底色（默认品牌青 `rgb(34,149,144)`；全键盘面板另带 1px 灰边，小键盘不带——这是两者既定差异，非遗漏）

`src/Keyboard.ui` 里的青底/logo 文本是 Designer 预览用默认值，运行期被上述常量覆盖。

## 文件 map

```
TouchKeyboard/
├── TouchKeyboard.pri        qmake 接入点（SOURCES/HEADERS/FORMS/RESOURCES）
├── README.md
├── src/
│   ├── AbstractKeyboard.h   基类：按键分发 + 品牌常量
│   ├── KeyButton.h/.cpp     自绘键帽（多形态/长按连发/图标）
│   ├── TouchKbdDict.h       三字典路径宏 + 打开兜底（26 键与 T9 共用）
│   ├── Keyboard.h/.cpp/.ui  26 键全键盘
│   ├── T9Keyboard.h/.cpp    九宫格文本键盘
│   ├── T9PinyinEngine.h/.cpp 九宫格拼音引擎（纯逻辑，后台加载）
│   ├── NumberKeyboard.h/.cpp 数字小键盘（0-9/-/./退格 + 清空/确认）
│   └── InputManager.h/.cpp  选弹/定位/收起、布局切换（numericInput 判据在这）
├── tests/t9/                T9 引擎单测
└── resources/
    ├── TouchKeyboard.qrc    图标资源（:/touchkeyboard/icons/ 前缀）
    └── icons/               backspace/caplock/enter/hideBtn/space（**已不再引用**，保留兼容）

键帽图标（退格/大小写/回车/空格/收起）一律由 `AbstractKeyboard.h` 的 `keyIcon()` 用 QPainter
现画，不读图片：RK3566 Buildroot 的 Qt 5.15.2 以 `-no-libpng` 构建，`QIcon(":/…png")` 读出空图，
全键盘这几枚键与数字小键盘退格整块空白（2026-10-26 aerocount 板上实测）。新增图标键照此办理，
别再引 png/svg。
```

## 已知边界

- 仅覆盖 QLineEdit/QTextEdit；QAbstractSpinBox（QSpinBox/QDoubleSpinBox）已沿父链补查
  一层拿到 numericInput 判据；**QComboBox 的可编辑行编辑尚未做**属性透传（按需再补）。
- QTextEdit 场景清空/确认同样生效，但未做多行差异处理（当前宿主无此场景）。
- 全键盘「回车」提交 = 失焦触发（与数字小键盘「确认」同路径）：QLineEdit 对改过的文本
  失焦会自行发一次 `editingFinished`；**未修改即回车/确认不会发**（与 Qt 硬件键盘的
  Return 直发路径有别——直发 Return 键事件时未修改也会发 EF）。

---

## Qt 版本兼容（移植记录，2026-09-24）

模板原为 Qt5 Widgets；PUPSIT（Qt 6）接入时做过一轮 API 迁移，`3a6cfe1` 起补齐
`QT_VERSION` 守卫后两版本同树编译（Qt6 分支逐字节不变，Qt5 分支回落旧 API）：

| Qt5 写法 | Qt6 写法 | 位置 |
|---|---|---|
| `#include <QRegExp>` + `QRegExp::exactMatch/indexIn/cap/matchedLength` | `#include <QRegularExpression>` + anchored 模式 `.match().hasMatch()` / `globalMatch()` + `captured()/capturedLength()` | KeyButton.cpp、Keyboard.cpp |
| `layout->setMargin(0)` | `layout->setContentsMargins(0, 0, 0, 0)` | NumberKeyboard.cpp |
| `font.setWeight(50)`（Qt5 0-99 制，=Normal） | `font.setWeight(QFont::Normal)` | Keyboard.cpp |
| `QTextStream::setCodec("UTF-16")` | `in.setEncoding(QStringConverter::Encoding::Utf16)`（include 与调用均以 `QT_VERSION >= 6` 守卫，5.15 回落 `setCodec`） | Keyboard.cpp |
| 缺 `QFile`/`QRegularExpression` include（Qt5 传递包含） | 显式补上 | Keyboard.cpp、KeyButton.cpp |
| `QVariant(空串).isNull()`（Qt5 为 true，Qt6 不再）→ KeyButton 构造里「未传 display 时用 value 兜底」失效，字母/数字键帽全空白 | `mode.display.isNull() \|\| mode.display.toString().isEmpty()` | KeyButton.cpp |

行为语义保持不变（anchored 模式等价 exactMatch；全局匹配迭代等价 indexIn 循环）。
