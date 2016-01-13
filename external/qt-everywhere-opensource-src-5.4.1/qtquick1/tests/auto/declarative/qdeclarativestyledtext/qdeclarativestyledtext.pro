CONFIG += testcase
TARGET = tst_qdeclarativestyledtext

QT += testlib declarative declarative-private
QT += network
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativestyledtext.cpp

# QMAKE_CXXFLAGS = -fprofile-arcs -ftest-coverage
# LIBS += -lgcov

CONFIG += parallel_test
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
