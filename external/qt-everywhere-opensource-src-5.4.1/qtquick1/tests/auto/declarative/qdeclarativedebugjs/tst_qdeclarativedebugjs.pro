CONFIG += testcase
testcase.timeout = 600 # this test is slow
TARGET = tst_qdeclarativedebugjs

QT += testlib declarative declarative-private network script core-private gui-private widgets-private
macx:CONFIG -= app_bundle

HEADERS += ../shared/debugutil_p.h

SOURCES += tst_qdeclarativedebugjs.cpp \
           ../shared/debugutil.cpp

include(../shared/qdeclarativedatatest.pri)

# QMAKE_CXXFLAGS = -fprofile-arcs -ftest-coverage
# LIBS += -lgcov

OTHER_FILES = data/backtrace1.js data/backtrace1.qml

CONFIG += parallel_test
CONFIG+=insignificant_test # QTQAINFRA-428
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
