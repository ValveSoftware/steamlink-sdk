TARGET = tst_qdeclarativevideooutput_window

QT += multimedia-private qml testlib quick
CONFIG += testcase

SOURCES += \
        tst_qdeclarativevideooutput_window.cpp

INCLUDEPATH += ../../../../src/imports/multimedia
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

win32:contains(QT_CONFIG, angle): CONFIG += insignificant_test # QTBUG-28541
