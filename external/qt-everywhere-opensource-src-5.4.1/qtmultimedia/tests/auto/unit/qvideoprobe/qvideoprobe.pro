CONFIG += testcase no_private_qt_headers_warning
TARGET = tst_qvideoprobe

QT += multimedia-private testlib

SOURCES += tst_qvideoprobe.cpp

include (../qmultimedia_common/mock.pri)
include (../qmultimedia_common/mockrecorder.pri)
include (../qmultimedia_common/mockplayer.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
