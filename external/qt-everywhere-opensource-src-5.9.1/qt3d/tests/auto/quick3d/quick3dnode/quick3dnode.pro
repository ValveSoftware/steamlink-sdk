TEMPLATE = app

TARGET = tst_quick3dnode

QT += 3dcore 3dcore-private 3drender 3drender-private 3dquick 3dquick-private 3dquickrender-private testlib

CONFIG += testcase

include(../../render/qmlscenereader/qmlscenereader.pri)

SOURCES += tst_quick3dnode.cpp

RESOURCES += quick3dnode.qrc
