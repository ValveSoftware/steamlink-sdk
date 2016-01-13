TARGET = tst_qxmlformatter
CONFIG += testcase
QT += testlib
SOURCES += tst_qxmlformatter.cpp

TESTDATA = baselines/* input/*

include (../xmlpatterns.pri)

wince*:{
   addFiles.files = baselines input
   addFiles.path = .
   DEPLOYMENT += addFiles
}
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
