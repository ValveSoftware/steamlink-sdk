CONFIG += testcase
TARGET = tst_qgeocodingmanager

HEADERS += ../utils/qlocationtestutils_p.h \
    tst_qgeocodingmanager.h

SOURCES += tst_qgeocodingmanager.cpp \
           ../utils/qlocationtestutils.cpp

QT += location testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
