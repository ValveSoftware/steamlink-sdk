TEMPLATE = app
CONFIG+=testcase
TARGET=tst_qgeopositioninfo

SOURCES += tst_qgeopositioninfo.cpp

QT += positioning testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
