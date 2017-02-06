TARGET = tst_qsoundeffect

QT += core multimedia-private testlib

# This is more of a system test
CONFIG += testcase

SOURCES += tst_qsoundeffect.cpp

unix:!mac {
    !qtConfig(pulseaudio) {
        DEFINES += QT_MULTIMEDIA_QMEDIAPLAYER
    }
}

TESTDATA += test.wav
