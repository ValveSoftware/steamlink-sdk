TARGET = tst_qsvgrenderer
CONFIG += testcase
QT += svg testlib widgets gui-private

SOURCES += tst_qsvgrenderer.cpp
RESOURCES += resources.qrc

DEFINES += SRCDIR=\\\"$$PWD/\\\"
