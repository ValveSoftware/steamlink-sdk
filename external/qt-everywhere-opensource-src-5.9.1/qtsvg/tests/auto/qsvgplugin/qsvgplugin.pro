TARGET = tst_qsvgplugin
CONFIG += testcase
QT += svg testlib widgets gui-private

SOURCES += tst_qsvgplugin.cpp
RESOURCES += resources.qrc

DEFINES += SRCDIR=\\\"$$PWD/\\\"
