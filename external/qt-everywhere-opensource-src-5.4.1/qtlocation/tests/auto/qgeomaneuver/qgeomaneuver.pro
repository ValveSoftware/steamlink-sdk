TEMPLATE = app
CONFIG+=testcase
TARGET=tst_qgeomaneuver

# Input
HEADERS += ../utils/qlocationtestutils_p.h \
           tst_qgeomaneuver.h
SOURCES += tst_qgeomaneuver.cpp \
           ../utils/qlocationtestutils.cpp

QT += location testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
