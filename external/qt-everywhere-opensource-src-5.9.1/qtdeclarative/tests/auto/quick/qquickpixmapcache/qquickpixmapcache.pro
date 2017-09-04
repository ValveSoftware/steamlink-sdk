CONFIG += testcase
TARGET = tst_qquickpixmapcache
macx:CONFIG -= app_bundle

SOURCES += tst_qquickpixmapcache.cpp \
           ../../shared/testhttpserver.cpp
HEADERS += ../../shared/testhttpserver.h
INCLUDEPATH += ../../shared/

include (../../shared/util.pri)

TESTDATA = data/*

# QMAKE_CXXFLAGS = -fprofile-arcs -ftest-coverage
# LIBS += -lgcov

QT += core-private gui-private qml-private quick-private network testlib concurrent
