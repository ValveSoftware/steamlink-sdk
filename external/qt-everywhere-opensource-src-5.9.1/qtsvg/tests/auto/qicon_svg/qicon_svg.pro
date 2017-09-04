CONFIG += testcase
TARGET = tst_qicon_svg

QT += svg testlib gui-private
SOURCES += tst_qicon_svg.cpp
RESOURCES = tst_qicon_svg.qrc

TESTDATA += icons/*
