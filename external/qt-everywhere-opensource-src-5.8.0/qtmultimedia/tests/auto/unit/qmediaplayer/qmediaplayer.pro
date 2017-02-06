CONFIG += testcase
TARGET = tst_qmediaplayer
QT += network multimedia-private testlib
SOURCES += tst_qmediaplayer.cpp
RESOURCES += testdata.qrc

include (../qmultimedia_common/mock.pri)
include (../qmultimedia_common/mockplayer.pri)
