# TouchKeyboard —— 触摸键盘模板

> 仓库：https://github.com/Notion0/TouchKeyboard （Qt5 Widgets / C++11 / MSVC·GCC 通吃）

全键盘（字母/符号/中文候选）+ 数字小键盘 + 全局 InputManager。Qt5 Widgets、C++11、MSVC(2019)/GCC 通吃。
从 PressureMonitor 项目抽出（2026-09-24），两键盘形态以生产巡检后的稳定版为准。

## 30 秒接入

1. 宿主 `.pro` 加一行（模板放宿主同级目录 `../TouchKeyboard` 时最省事；也可绝对路径）：
   ```qmake
   include(../TouchKeyboard/TouchKeyboard.pri)
   ```
2. 主窗口创建后调用一次：
   ```cpp
   #include "InputManager.h"          // 模板 src/ 已在 INCLUDEPATH，无需带目录前缀
   InputManager::Instance()->Init(this);
   ```
3. 给**纯数字**输入框打动态属性（决定弹小键盘还是全键盘）：
   - `.ui` 里：
     ```xml
     <property name="numericInput" stdset="0">
      <bool>true</bool>
     </property>
     ```
   - 代码里：`edit->setProperty("numericInput", true);`
   - ⚠ **`stdset="0"` 不可省**：uic 对无此标记的属性会按"标准属性"直接生成 `setNumericInput(true)` 编译失败（踩过）；带上才生成 `setProperty()` 动态属性调用。
   不标的输入框（用户名/批号/标题等）弹全键盘。

## 行为参考

| | 全键盘 | 数字小键盘 |
|---|---|---|
| 大小 | 990×300（标题栏 40 + 4 行键） | 330×355（标题栏 34 + 4 行 65px + 动作行 65px） |
| 弹出条件 | 焦点进入任意 QLineEdit/QTextEdit | 同上且 `numericInput==true`（两键盘互斥） |
| 提交 | **回车**键（editingFinished + 失焦 + 收起） | **确认**键（同语义；小键盘无回车键必须有） |
| 清空 | 底行**清空**键（清拼音缓冲 + 清空输入框） | 底行**清空**键（清空输入框） |
| 收起 | 点击非输入区 / 失焦 / 标题栏关闭钮 | 点击非输入区 / 失焦 / 确认 |
| 中文 | 中/英切换 + 拼音候选行（需字典，见下） | 无 |
| 位置 | 输入框下方，越界自动上翻/贴边 | 同 |

按键统一走 `AbstractKeyboard::onKeyPressed` 的 QKeyEvent 通道发到焦点控件；两个键盘都是
无焦点置顶 Tool 窗（`WA_ShowWithoutActivating` + `WindowDoesNotAcceptFocus`），点键不抢焦点。

## 中文输入（可选）

三字典由宿主工程用同名 qrc 路径提供，**缺失时自动降级**（load 失败打 qDebug 后返回，键盘其余功能正常）：

| 宏（可 -D 覆盖） | 默认路径 | 内容 |
|---|---|---|
| `TOUCHKBD_PINYIN_DICT` | `:/ChineseLib/pinyin.txt` | 单字拼音 |
| `TOUCHKBD_PINYIN_PHRASE_DICT` | `:/ChinesePhraseLib/pinyin_phrase.txt` | 词组 |
| `TOUCHKBD_GOOGLE_DICT` | `:/GoogleChineseLib/rawdict_utf16_65105_freq_sort.txt` | 谷歌大字典（~8MB，词频排序） |

不需要中文的宿主：什么都不用做。

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
│   ├── Keyboard.h/.cpp/.ui  全键盘
│   ├── NumberKeyboard.h/.cpp 数字小键盘（0-9/./退格 + 清空/确认）
│   └── InputManager.h/.cpp  双键盘选弹/定位/收起（numericInput 判据在这）
└── resources/
    ├── TouchKeyboard.qrc    图标资源（:/touchkeyboard/icons/ 前缀）
    └── icons/               backspace/caplock/enter/hideBtn/space
```

## 已知边界

- 仅覆盖 QLineEdit/QTextEdit；QComboBox/QSpinBox 的内嵌编辑框未做属性透传（PUPSIT 版有，按需再移植）。
- QTextEdit 场景清空/确认同样生效，但未做多行差异处理（当前宿主无此场景）。
