CONFIG += testcase
TARGET = tst_qcamerawidgets

QT += multimedia-private multimediawidgets-private testlib

include (../qmultimedia_common/mock.pri)
include (../qmultimedia_common/mockcamera.pri)

SOURCES += tst_qcamerawidgets.cpp

QT+=widgets
