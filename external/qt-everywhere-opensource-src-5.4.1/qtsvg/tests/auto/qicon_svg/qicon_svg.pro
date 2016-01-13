CONFIG += testcase
TARGET = tst_qicon_svg

QT += testlib gui-private
SOURCES += tst_qicon_svg.cpp
RESOURCES = tst_qicon_svg.qrc

wince* {
   QT += svg
   DEPLOYMENT_PLUGIN += qsvg
}
TESTDATA += icons/*
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
