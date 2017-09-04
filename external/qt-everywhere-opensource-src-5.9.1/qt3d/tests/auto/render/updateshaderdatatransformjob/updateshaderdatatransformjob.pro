TEMPLATE = app

TARGET = tst_updateshaderdatatransformjob

QT += 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_updateshaderdatatransformjob.cpp

include(../../core/common/common.pri)
include(../commons/commons.pri)
include(../qmlscenereader/qmlscenereader.pri)
# Extra dependencies to build test scenes needed by the tests
QT += quick 3dquick 3dquick-private 3dextras 3dquickextras

RESOURCES += \
    updateshaderdatatransformjob.qrc
