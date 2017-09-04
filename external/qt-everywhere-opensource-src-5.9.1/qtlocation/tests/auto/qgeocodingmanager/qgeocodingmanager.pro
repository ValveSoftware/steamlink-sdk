CONFIG += testcase
TARGET = tst_qgeocodingmanager

HEADERS += ../utils/qlocationtestutils_p.h \
    tst_qgeocodingmanager.h

SOURCES += tst_qgeocodingmanager.cpp \
           ../utils/qlocationtestutils.cpp

CONFIG -= app_bundle

QT += location testlib
