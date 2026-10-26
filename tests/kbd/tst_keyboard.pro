# 键盘控件单测（2026-10-26）：现画键帽图标、九宫格密码框字母直选、全键盘底行缩字。
# 用法：cd tests/kbd && qmake && make && QT_QPA_PLATFORM=offscreen ./tst_keyboard
QT += core gui widgets testlib
CONFIG += console c++17 testcase
CONFIG -= app_bundle
TARGET = tst_keyboard

include(../../TouchKeyboard.pri)
SOURCES += tst_keyboard.cpp
