TEMPLATE = app

TARGET = tst_import3drender

QT += 3dcore 3dcore-private 3drender 3drender-private testlib 3dquick

CONFIG += testcase

SOURCES += tst_import3drender.cpp

include(../../render/qmlscenereader/qmlscenereader.pri)

RESOURCES += 3drender.qrc
