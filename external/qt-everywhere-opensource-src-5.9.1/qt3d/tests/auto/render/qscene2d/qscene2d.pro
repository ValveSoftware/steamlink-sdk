TEMPLATE = app

TARGET = qscene2d

QT += 3dcore 3dcore-private 3drender 3drender-private testlib 3dquickscene2d 3dquickscene2d-private

CONFIG += testcase

SOURCES += tst_qscene2d.cpp

include(../../core/common/common.pri)
include(../commons/commons.pri)
