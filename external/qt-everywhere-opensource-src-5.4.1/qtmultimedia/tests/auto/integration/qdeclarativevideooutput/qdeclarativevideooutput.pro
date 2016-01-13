TARGET = tst_qdeclarativevideooutput

QT += multimedia-private qml testlib quick
CONFIG += testcase

SOURCES += \
        tst_qdeclarativevideooutput.cpp

INCLUDEPATH += ../../../../src/imports/multimedia
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
