CONFIG += testcase
TARGET = tst_qdeclarativedebugobservermode

QT += testlib declarative declarative-private network script core-private gui-private widgets-private
macx:CONFIG -= app_bundle

HEADERS += ../shared/debugutil_p.h

SOURCES += tst_qdeclarativedebugobservermode.cpp \
           ../shared/debugutil.cpp

include(../shared/qdeclarativedatatest.pri)

# QMAKE_CXXFLAGS = -fprofile-arcs -ftest-coverage
# LIBS += -lgcov

OTHER_FILES = data/qtquick1.qml

CONFIG += parallel_test
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

CONFIG+=insignificant_test # QTBUG-28298
