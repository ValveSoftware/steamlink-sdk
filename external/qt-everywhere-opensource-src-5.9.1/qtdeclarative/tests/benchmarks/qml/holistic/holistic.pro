CONFIG += benchmark
TEMPLATE = app
TARGET = tst_holistic
QT += qml network testlib
macx:CONFIG -= app_bundle

CONFIG += release

SOURCES += tst_holistic.cpp \
           testtypes.cpp
HEADERS += testtypes.h

DEFINES += SRCDIR=\\\"$$PWD\\\"
