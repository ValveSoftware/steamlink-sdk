TEMPLATE = app
CONFIG+=testcase
testcase.timeout = 400 # this test is slow
QT += network positioning testlib
TARGET = tst_qnmeapositioninfosource_realtime_generic

INCLUDEPATH += ..

HEADERS += ../../utils/qlocationtestutils_p.h \
           ../../qgeopositioninfosource/testqgeopositioninfosource_p.h \
           ../qnmeapositioninfosourceproxyfactory.h \
           ../tst_qnmeapositioninfosource.h

SOURCES += ../../utils/qlocationtestutils.cpp \
           ../../qgeopositioninfosource/testqgeopositioninfosource.cpp \
           ../qnmeapositioninfosourceproxyfactory.cpp \
           ../tst_qnmeapositioninfosource.cpp \
           tst_qnmeapositioninfosource_realtime_generic.cpp

CONFIG -= app_bundle

# This test relies on a working local QTcpSocket(Server). When the CI is under
# heavy load the socket code cannot establish a connection which leads to flaky
# test results. We make this test insiginficant as there is currently no known
# solution to this problem. On the positive side QNmeaPositionInfoSource
# does not have a platform specific implementation. Other platforms should provide
# a close enough test approximation.
win32:CONFIG+=insignificant_test
