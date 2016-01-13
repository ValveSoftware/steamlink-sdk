CONFIG += testcase
TARGET = tst_qdeclarativeaudio

QT += multimedia-private qml testlib

HEADERS += \
        ../../../../src/imports/multimedia/qdeclarativeaudio_p.h \
        ../../../../src/imports/multimedia/qdeclarativemediametadata_p.h

SOURCES += \
        tst_qdeclarativeaudio.cpp \
        ../../../../src/imports/multimedia/qdeclarativeaudio.cpp

INCLUDEPATH += ../../../../src/imports/multimedia
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
