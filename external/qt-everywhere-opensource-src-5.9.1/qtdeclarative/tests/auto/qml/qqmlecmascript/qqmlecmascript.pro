CONFIG += testcase
TARGET = tst_qqmlecmascript
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlecmascript.cpp \
           testtypes.cpp \
           ../../shared/testhttpserver.cpp
HEADERS += testtypes.h \
           ../../shared/testhttpserver.h
INCLUDEPATH += ../../shared

RESOURCES += qqmlecmascript.qrc

include (../../shared/util.pri)

# QMAKE_CXXFLAGS = -fprofile-arcs -ftest-coverage
# LIBS += -lgcov

TESTDATA = data/*

QT += core-private gui-private  qml-private network testlib
qtHaveModule(widgets): QT += widgets
