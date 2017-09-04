QT = core testlib serialbus
TARGET = tst_qcanbus
CONFIG += testcase
CONFIG -= app_bundle

QTPLUGIN += qtcanbustestgeneric

SOURCES += tst_qcanbus.cpp
