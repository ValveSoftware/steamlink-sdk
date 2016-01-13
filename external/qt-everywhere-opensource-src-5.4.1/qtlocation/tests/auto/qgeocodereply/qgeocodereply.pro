TEMPLATE = app
CONFIG += testcase
TARGET = tst_qgeocodereply

HEADERS += ../utils/qlocationtestutils_p.h \
           tst_qgeocodereply.h
SOURCES += tst_qgeocodereply.cpp \
           ../utils/qlocationtestutils.cpp

QT += location testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
