CONFIG += testcase
TARGET = tst_qgeolocation

HEADERS += ../utils/qlocationtestutils_p.h \
           tst_qgeolocation.h
SOURCES += tst_qgeolocation.cpp \
           ../utils/qlocationtestutils.cpp

QT += positioning testlib
