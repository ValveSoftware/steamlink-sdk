TEMPLATE = app
TARGET = tst_startup

CONFIG += console
macos:CONFIG -= app_bundle

SOURCES += \
    startup_bench.cpp

RESOURCES += \
    gallery.qrc

include(../../../examples/quickcontrols/controls/shared/shared.pri)
