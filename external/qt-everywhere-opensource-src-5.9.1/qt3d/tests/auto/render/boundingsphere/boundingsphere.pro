TEMPLATE = app

TARGET = tst_boundingsphere
QT += core-private 3dcore 3dcore-private 3drender 3drender-private 3dextras testlib

CONFIG += testcase

SOURCES += tst_boundingsphere.cpp

include(../commons/commons.pri)
include(../../core/common/common.pri)
include(../qmlscenereader/qmlscenereader.pri)
# Extra dependencies to build test scenes needed by the tests
QT += quick 3dquick 3dquick-private 3dextras 3dquickextras

RESOURCES += \
    boundingsphere.qrc
