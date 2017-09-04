CONFIG += testcase
TARGET = tst_qmediaplayerwidgets
QT += network multimedia-private multimediawidgets-private testlib widgets
SOURCES += tst_qmediaplayerwidgets.cpp

include (../qmultimedia_common/mock.pri)
include (../qmultimedia_common/mockplayer.pri)
