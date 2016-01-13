TEMPLATE = app
CONFIG+=testcase
testcase.timeout = 400 # this test is slow
TARGET=tst_qgeopositioninfosource

HEADERS += ../utils/qlocationtestutils_p.h \
           testqgeopositioninfosource_p.h

SOURCES += ../utils/qlocationtestutils.cpp \
           testqgeopositioninfosource.cpp \
           tst_qgeopositioninfosource.cpp

QT += positioning testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
