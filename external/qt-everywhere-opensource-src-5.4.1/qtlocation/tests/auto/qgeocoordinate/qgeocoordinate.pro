CONFIG += testcase
TARGET = tst_qgeocoordinate

HEADERS += ../utils/qlocationtestutils_p.h
SOURCES += tst_qgeocoordinate.cpp \
           ../utils/qlocationtestutils.cpp

QT += positioning testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
