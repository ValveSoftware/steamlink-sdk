CONFIG += testcase
TARGET = tst_qdeclarativeqt

QT += testlib declarative declarative-private script script-private core-private widgets
SOURCES += tst_qdeclarativeqt.cpp
macx:CONFIG -= app_bundle

include(../shared/qdeclarativedatatest.pri)

# QMAKE_CXXFLAGS = -fprofile-arcs -ftest-coverage
# LIBS += -lgcov

CONFIG += parallel_test
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
