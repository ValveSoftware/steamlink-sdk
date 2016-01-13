TARGET = tst_qsoundeffect

QT += core multimedia-private testlib

# This is more of a system test
CONFIG += testcase

SOURCES += tst_qsoundeffect.cpp

unix:!mac {
    !contains(QT_CONFIG, pulseaudio) {
        DEFINES += QT_MULTIMEDIA_QMEDIAPLAYER
    }
}

TESTDATA += test.wav

win32:CONFIG += insignificant_test # QTBUG-26509
linux-*:CONFIG += insignificant_test # QTBUG-26748
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
