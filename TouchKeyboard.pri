# TouchKeyboard.pri —— 触摸键盘模板：全键盘（字母/符号/中文候选）+ 数字小键盘 + InputManager
#
# 宿主工程用法（.pro 内）：
#   include(../TouchKeyboard/TouchKeyboard.pri)      # 相对路径（模板放宿主同级目录时最省事）
#   或 include(/home/yao/TouchKeyboard/TouchKeyboard.pri)   # 绝对路径
#
# 依赖：Qt5 Widgets（Qt 5.15 验证；信号槽用新语法、无 C++11 之后特性）。
# 资源：模板自带 5 枚图标（qrc 前缀 :/touchkeyboard/icons/），拷走即用。
# 中文输入：三字典已随本模板 qrc 编入（resources/dicts/，见 README「中文输入」），宿主零配置即得。
# 换肤：改 src/AbstractKeyboard.h 的 brandLogoText() / brandPanelColor() 一处，两个键盘同时生效。

TOUCHKBD_ROOT = $$PWD

INCLUDEPATH += $$TOUCHKBD_ROOT/src

HEADERS += \
    $$TOUCHKBD_ROOT/src/AbstractKeyboard.h \
    $$TOUCHKBD_ROOT/src/KeyButton.h \
    $$TOUCHKBD_ROOT/src/Keyboard.h \
    $$TOUCHKBD_ROOT/src/NumberKeyboard.h \
    $$TOUCHKBD_ROOT/src/InputManager.h

SOURCES += \
    $$TOUCHKBD_ROOT/src/KeyButton.cpp \
    $$TOUCHKBD_ROOT/src/Keyboard.cpp \
    $$TOUCHKBD_ROOT/src/NumberKeyboard.cpp \
    $$TOUCHKBD_ROOT/src/InputManager.cpp

FORMS += $$TOUCHKBD_ROOT/src/Keyboard.ui

RESOURCES += $$TOUCHKBD_ROOT/resources/TouchKeyboard.qrc
