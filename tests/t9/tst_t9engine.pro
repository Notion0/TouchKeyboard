# T9 拼音引擎单测：直接编引擎源文件 + 模板 qrc（三字典）。
# 用法：cd tests/t9 && qmake && make && ./tst_t9engine
QT += core testlib
QT -= gui
CONFIG += console c++17 testcase
CONFIG -= app_bundle
TARGET = tst_t9engine

TOUCHKBD_ROOT = $$PWD/../..
INCLUDEPATH += $$TOUCHKBD_ROOT/src

HEADERS += $$TOUCHKBD_ROOT/src/T9PinyinEngine.h \
           $$TOUCHKBD_ROOT/src/TouchKbdDict.h
SOURCES += tst_t9engine.cpp \
           $$TOUCHKBD_ROOT/src/T9PinyinEngine.cpp
RESOURCES += $$TOUCHKBD_ROOT/resources/TouchKeyboard.qrc
