CONFIG += testcase
TARGET = tst_qcameraviewfinder

QT += multimedia-private multimediawidgets-private testlib

include (../qmultimedia_common/mock.pri)
include (../qmultimedia_common/mockcamera.pri)

SOURCES += tst_qcameraviewfinder.cpp
QT+=widgets
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
